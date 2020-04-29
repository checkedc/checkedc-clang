//=--CheckedCConvert.cpp------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
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
#include "ArrayBoundsInferenceConsumer.h"
#include "IterativeItypeHelper.h"

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

cl::opt<bool> MergeMultipleFuncDecls("mergefds",
                                     cl::desc("Merge multiple declarations of "
                                              "functions."),
                                     cl::init(false),
                                     cl::cat(ConvertCategory));

static cl::opt<std::string>
    OutputPostfix("output-postfix",
                  cl::desc("Postfix to add to the names of rewritten files, if "
                           "not supplied writes to STDOUT"),
                  cl::init("-"), cl::cat(ConvertCategory));

static cl::opt<std::string>
  ConstraintOutputJson("constraint-output",
                       cl::desc("Path to the file where all the analysis "
                                  "information will be dumped as json"),
                       cl::init("constraint_output.json"),
                         cl::cat(ConvertCategory));

static cl::opt<bool> DumpStats( "dump-stats",
                                cl::desc("Dump statistics"),
                                cl::init(false),
                                cl::cat(ConvertCategory));

cl::opt<bool> HandleVARARGS( "handle-varargs",
                             cl::desc("Enable handling of varargs in a "
                                     "sound manner"),
                             cl::init(false),
                             cl::cat(ConvertCategory));

cl::opt<bool> EnablePropThruIType( "enable-itypeprop",
                                   cl::desc("Enable propagation of "
                                           "constraints through ityped "
                                           "parameters/returns."),
                                   cl::init(false),
                                   cl::cat(ConvertCategory));

cl::opt<bool> AllocUnsafe( "alloc-unsafe",
                          cl::desc("Consider the allocators "
                                       "(i.e., malloc/calloc) as unsafe."),
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
    return std::unique_ptr<ASTConsumer>(new T(Info,
                                              &Compiler.getASTContext()));
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
      (new T(Info, Files, &Compiler.getASTContext(),
               OutputPostfix, BaseDir));
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


std::pair<Constraints::ConstraintSet, bool>
    solveConstraintsWithFunctionSubTyping(ProgramInfo &Info) {
  // Solve the constrains by handling function sub-typing.
  Constraints &CS = Info.getConstraints();
  unsigned NumIter = 0;
  std::pair<Constraints::ConstraintSet, bool> Ret;
  bool Fixed = false;
  while (!Fixed) {
    Ret = CS.solve(NumIter);
    if (NumIter > 1)
      // This means we have made some changes to the environment
      // see if the function subtype handling causes any changes?
      Fixed = !Info.handleFunctionSubtyping();
    else
      // we reached a fixed point.
      Fixed = true;
  }
  return Ret;
}

bool performIterativeItypeRefinement(Constraints &CS, ProgramInfo &Info,
                                     ClangTool &Tool, std::set<std::string>
                                         &InoutPaths) {
  bool FixedPoint = false;
  unsigned long IterNum = 1;
  unsigned long EdgesRemoved = 0;
  unsigned long NumItypeVars = 0;
  std::set<std::string> ModFunctions;
  if (Verbose) {
    errs() << "Trying to capture Constraint Variables for all functions\n";
  }
  // First capture itype parameters and return values for all functions.
  performConstraintSetup(Info);

  // Sanity check.
  assert(CS.checkInitialEnvSanity() && "Invalid initial environment. "
                                       "We expect all pointers to be "
                                       "initialized with Ptr to begin with.");

  while (!FixedPoint) {
    clock_t StartTime = clock();
    errs() << "****Iteration " << IterNum << " starts.****\n";
    if (Verbose) {
      errs() << "Iterative Itype refinement, Round:" << IterNum << "\n";
    }

    std::pair<Constraints::ConstraintSet, bool> R =
        solveConstraintsWithFunctionSubTyping(Info);

    errs() << "Iteration:" << IterNum
           << ", Constraint solve time:" <<
        getTimeSpentInSeconds(StartTime) << "\n";

    if (R.second) {
      errs() << "Constraints solved for iteration:" << IterNum << "\n";
    }

    if (DumpStats) {
      Info.print_stats(InoutPaths, llvm::errs(), true);
    }

    // Get all the functions whose constraints have been modified.
    identifyModifiedFunctions(CS, ModFunctions);

    StartTime = clock();
    // Detect and update new found itype vars.
    NumItypeVars = detectAndUpdateITypeVars(Info, ModFunctions);

    errs() << "Iteration:" << IterNum
           << ", Number of detected itype vars:"
           << NumItypeVars
           << ", detection time:" <<
        getTimeSpentInSeconds(StartTime) << "\n";

    StartTime = clock();
    // Update the constraint graph by removing edges from/to iype parameters
    // and returns.
    EdgesRemoved = resetWithitypeConstraints(CS);

    errs() << "Iteration:" << IterNum
           << ", Number of edges removed:" << EdgesRemoved << "\n";

    errs() << "Iteration:" << IterNum
           << ", Refinement Time:" <<
        getTimeSpentInSeconds(StartTime) << "\n";

    // If we removed any edges, that means we did not reach fix point.
    // In other words, we reach fixed point when no edges are removed from the
    // constraint graph.
    FixedPoint = !(EdgesRemoved > 0);
    errs() << "****Iteration " << IterNum << " ends****\n";
    IterNum++;
  }

  errs() << "Fixed point reached after " << (IterNum -1) <<
      " iterations.\n";

  return FixedPoint;
}

int main(int argc, const char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);

  // Initialize targets for clang module support.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  if (BaseDir.size() == 0) {
    SmallString<256> Cp;
    if (std::error_code E = sys::fs::current_path(Cp)) {
      errs() << "could not get current working dir\n";
      return 1;
    }

    BaseDir = Cp.str();
  }

  CommonOptionsParser OptionsParser(argc, argv, ConvertCategory);

  tooling::CommandLineArguments Args = OptionsParser.getSourcePathList();

  ClangTool Tool(OptionsParser.getCompilations(), Args);
  std::set<std::string> InoutPaths;

  for (const auto &S : Args) {
    std::string AbsPath;
    if (getAbsoluteFilePath(S, AbsPath))
      InoutPaths.insert(AbsPath);
  }

  if (OutputPostfix == "-" && InoutPaths.size() > 1) {
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

  Constraints &CS = Info.getConstraints();

  // perform constraint solving by iteratively refining based on itypes.
  bool FixPoint = performIterativeItypeRefinement(CS, Info,
                                                  Tool, InoutPaths);

  assert(FixPoint == true);
  if (Verbose)
    outs() << "Constraints solved\n";
  if (DumpIntermediate) {
    Info.dump();
    outs() << "Writing json output to:" << ConstraintOutputJson << "\n";
    std::error_code Ec;
    llvm::raw_fd_ostream OutputJson(ConstraintOutputJson, Ec);
    if (!OutputJson.has_error()) {
      Info.dump_json(OutputJson);
      OutputJson.close();
    } else {
      Info.dump_json(llvm::errs());
    }
  }

  // 3. Re-write based on constraints.
  std::unique_ptr<ToolAction> RewriteTool =
      newFrontendActionFactoryB
      <RewriteAction<RewriteConsumer, ProgramInfo, std::set<std::string>>>(
          Info, InoutPaths);

  if (RewriteTool)
    Tool.run(RewriteTool.get());
  else
    llvm_unreachable("No action");

  if (DumpStats)
    Info.dump_stats(InoutPaths);

  return 0;
}
