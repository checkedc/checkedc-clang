//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"

#include <algorithm>

#include "Constraints.h"

#include "ConstraintBuilder.h"
#include "ProgramInfo.h"
#include "MappingVisitor.h"
#include "RewriteUtils.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("");

static cl::OptionCategory ConvertCategory("checked-c-convert options");
cl::opt<bool> DumpIntermediate( "dump-intermediate",
                                cl::desc("Dump intermediate information"),
                                cl::init(false),
                                cl::cat(ConvertCategory));

cl::opt<bool> Verbose("verbose",
                      cl::desc("Print verbose information"),
                      cl::init(false),
                      cl::cat(ConvertCategory));

cl::opt<bool> mergeMultipleFuncDecls("mergefds",
                                     cl::desc("Merge multiple declarations of functions."),
                                     cl::init(false),
                                     cl::cat(ConvertCategory));

static cl::opt<std::string>
    OutputPostfix("output-postfix",
                  cl::desc("Postfix to add to the names of rewritten files, if "
                           "not supplied writes to STDOUT"),
                  cl::init("-"), cl::cat(ConvertCategory));

static cl::opt<std::string>
  ConstraintOutputJson("constraint-output",
                       cl::desc("Path to the file where all the analysis information will be dumped as json"),
                       cl::init("constraint_output.json"), cl::cat(ConvertCategory));

static cl::opt<bool> DumpStats( "dump-stats",
                                cl::desc("Dump statistics"),
                                cl::init(false),
                                cl::cat(ConvertCategory));

cl::opt<bool> handleVARARGS( "handle-varargs",
                             cl::desc("Enable handling of varargs in a sound manner"),
                             cl::init(false),
                             cl::cat(ConvertCategory));

cl::opt<bool> enablePropThruIType( "enable-itypeprop",
                                   cl::desc("Enable propagation of constraints through ityped parameters/returns."),
                                   cl::init(false),
                                   cl::cat(ConvertCategory));

cl::opt<bool> considerAllocUnsafe( "alloc-unsafe",
                                   cl::desc("Consider the allocators (i.e., malloc/calloc) as unsafe."),
                                   cl::init(false),
                                   cl::cat(ConvertCategory));

static cl::opt<std::string>
BaseDir("base-dir",
  cl::desc("Base directory for the code we're translating"),
  cl::init(""),
  cl::cat(ConvertCategory));


template <typename T, typename V>
class GenericAction : public ASTFrontendAction {
public:
  GenericAction(V &I) : Info(I) {}

  virtual std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) {
    return std::unique_ptr<ASTConsumer>(new T(Info, &Compiler.getASTContext()));
  }

private:
  V &Info;
};

template <typename T, typename V, typename U>
class RewriteAction : public ASTFrontendAction {
public:
  RewriteAction(V &I, U &P) : Info(I),Files(P) {}

  virtual std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) {
    return std::unique_ptr<ASTConsumer>
      (new T(Info, Files, &Compiler.getASTContext(), OutputPostfix, BaseDir));
  }

private:
  V &Info;
  U &Files;
};

template <typename T>
std::unique_ptr<FrontendActionFactory>
newFrontendActionFactoryA(ProgramInfo &I) {
  class ArgFrontendActionFactory : public FrontendActionFactory {
  public:
    explicit ArgFrontendActionFactory(ProgramInfo &I) : Info(I) {}

    FrontendAction *create() override { return new T(Info); }

  private:
    ProgramInfo &Info;
  };

  return std::unique_ptr<FrontendActionFactory>(
      new ArgFrontendActionFactory(I));
}

template <typename T>
std::unique_ptr<FrontendActionFactory>
newFrontendActionFactoryB(ProgramInfo &I, std::set<std::string> &PS) {
  class ArgFrontendActionFactory : public FrontendActionFactory {
  public:
    explicit ArgFrontendActionFactory(ProgramInfo &I,
      std::set<std::string> &PS) : Info(I),Files(PS) {}

    FrontendAction *create() override { return new T(Info, Files); }

  private:
    ProgramInfo &Info;
    std::set<std::string> &Files;
  };

  return std::unique_ptr<FrontendActionFactory>(
    new ArgFrontendActionFactory(I, PS));
}

std::pair<Constraints::ConstraintSet, bool> solveConstraintsWithFunctionSubTyping(ProgramInfo &Info) {
  // solve the constrains by handling function sub-typing.
  Constraints &CS = Info.getConstraints();
  unsigned numIterations = 0;
  std::pair<Constraints::ConstraintSet, bool> toRet;
  bool fixed = false;
  while (!fixed) {
    toRet = CS.solve(numIterations);
    if (numIterations > 1)
      // this means we have made some changes to the environment
      // see if the function subtype handling causes any changes?
      fixed = !Info.handleFunctionSubtyping();
    else
      // we reached a fixed point.
      fixed = true;
  }
  return toRet;
}

int main(int argc, const char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);

  // Initialize targets for clang module support.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  if (BaseDir.size() == 0) {
    SmallString<256>  cp;
    if (std::error_code ec = sys::fs::current_path(cp)) {
      errs() << "could not get current working dir\n";
      return 1;
    }

    BaseDir = cp.str();
  }

  CommonOptionsParser OptionsParser(argc, argv, ConvertCategory);

  tooling::CommandLineArguments args = OptionsParser.getSourcePathList();

  ClangTool Tool(OptionsParser.getCompilations(), args);
  std::set<std::string> inoutPaths;

  for (const auto &S : args) {
    std::string abs_path;
    if (getAbsoluteFilePath(S, abs_path))
      inoutPaths.insert(abs_path);
  }

  if (OutputPostfix == "-" && inoutPaths.size() > 1) {
    errs() << "If rewriting more than one , can't output to stdout\n";
    return 1;
  }

  ProgramInfo Info;

  // 1. Gather constraints.
  std::unique_ptr<ToolAction> ConstraintTool = newFrontendActionFactoryA<
      GenericAction<ConstraintBuilderConsumer, ProgramInfo>>(Info);
  
  if (ConstraintTool)
    Tool.run(ConstraintTool.get());
  else
    llvm_unreachable("No action");

  if (!Info.link()) {
    errs() << "Linking failed!\n";
    return 1;
  }

  // 2. Solve constraints.
  if (Verbose)
    outs() << "Solving constraints\n";
  std::pair<Constraints::ConstraintSet, bool> R = solveConstraintsWithFunctionSubTyping(Info);
  // TODO: In the future, R.second will be false when there's a conflict, 
  //       and the tool will need to do something about that. 
  assert(R.second == true);
  if (Verbose)
    outs() << "Constraints solved\n";
  if (DumpIntermediate) {
    Info.dump();
    outs() << "Writing json output to:" << ConstraintOutputJson << "\n";
    std::error_code ec;
    llvm::raw_fd_ostream output_json(ConstraintOutputJson, ec);
    if (!output_json.has_error()) {
      Info.dump_json(output_json);
      output_json.close();
    } else {
      Info.dump_json(llvm::errs());
    }
  }

  // 3. Re-write based on constraints.
  std::unique_ptr<ToolAction> RewriteTool =
      newFrontendActionFactoryB
      <RewriteAction<RewriteConsumer, ProgramInfo, std::set<std::string>>>(
          Info, inoutPaths);
  
  if (RewriteTool)
    Tool.run(RewriteTool.get());
  else
    llvm_unreachable("No action");

  if (DumpStats)
    Info.dump_stats(inoutPaths);

  return 0;
}
