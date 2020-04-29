//=--CheckedCConvert.cpp------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// CConv tool
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
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"

#include <algorithm>

#include "Constraints.h"

#include "ConstraintBuilder.h"
#include "ProgramInfo.h"
#include "MappingVisitor.h"
#include "RewriteUtils.h"
#include "IterativeItypeHelper.h"
#include "CConvInteractive.h"
#include "GatherTool.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;
static cl::OptionCategory ConvertCategory("cconv options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("");

#ifdef CCCONVSTANDALONE

cl::opt<bool> DumpIntermediate( "dump-intermediate",
                                cl::desc("Dump intermediate information"),
                                cl::init(false),
                                cl::cat(ConvertCategory));

cl::opt<bool> Verbose("verbose",
                      cl::desc("Print verbose information"),
                      cl::init(false),
                      cl::cat(ConvertCategory));

cl::opt<bool> SeperateMultipleFuncDecls("seperatefds",
                                     cl::desc("Do not merge multiple "
                                                 "declarations of functions."),
                                     cl::init(false),
                                     cl::cat(ConvertCategory));

static cl::opt<std::string>
    OutputPostfix("output-postfix",
                  cl::desc("Postfix to add to the names of rewritten "
                           "files, if not supplied writes to STDOUT"),
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

cl::opt<bool> ConsiderAllocUnsafe( "alloc-unsafe",
                                   cl::desc("Consider the allocators "
                                           "(i.e., malloc/calloc) as unsafe."),
                                   cl::init(false),
                                   cl::cat(ConvertCategory));
cl::opt<bool> AllTypes( "alltypes",
                         cl::desc("Consider all Checked C types for "
                                "conversion"),
                         cl::init(false),
                         cl::cat(ConvertCategory));

cl::opt<bool> AddCheckedRegions( "addcr",
                                 cl::desc("Add Checked Regions"),
                                 cl::init(false),
                                 cl::cat(ConvertCategory));

cl::opt<std::string>
BaseDir("base-dir",
  cl::desc("Base directory for the code we're translating"),
  cl::init(""),
  cl::cat(ConvertCategory));

#else

bool DumpIntermediate;

bool Verbose;

bool SeperateMultipleFuncDecls;

std::string OutputPostfix;

std::string ConstraintOutputJson;

bool DumpStats;

bool HandleVARARGS;

bool EnablePropThruIType;

bool ConsiderAllocUnsafe;

bool AllTypes;

std::string BaseDir;

bool AddCheckedRegions;

#endif

// Suffixes for constraint output files.
#define INITIAL_OUTPUT_SUFFIX "_initial_constraints"
#define FINAL_OUTPUT_SUFFIX "_final_output"
#define BEFORE_SOLVING_SUFFIX "_before_solving_"
#define AFTER_SUBTYPING_SUFFIX "_after_subtyping_"

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

template <typename T, typename V>
class RewriteAction : public ASTFrontendAction {
public:
  RewriteAction(V &I) : Info(I) {}

  virtual std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) {
    return std::unique_ptr<ASTConsumer>
      (new T(Info, &Compiler.getASTContext(), OutputPostfix));
  }

private:
  V &Info;
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

void dumpConstraintOutputJson(const std::string &PostfixStr,
                              ProgramInfo &Info) {
  if (DumpIntermediate) {
    std::string FilePath = ConstraintOutputJson + PostfixStr + ".json";
    errs() << "Writing json output to:" << FilePath << "\n";
    std::error_code Ec;
    llvm::raw_fd_ostream OutputJson(FilePath, Ec);
    if (!OutputJson.has_error()) {
      Info.dump_json(OutputJson);
      OutputJson.close();
    } else {
      Info.dump_json(llvm::errs());
    }
  }
}

std::pair<Constraints::ConstraintSet, bool>
    solveConstraintsWithFunctionSubTyping(ProgramInfo &Info,
                                      unsigned IterID) {
  // Solve the constrains by handling function sub-typing.
  Constraints &CS = Info.getConstraints();
  unsigned NumIter = 0;
  std::pair<Constraints::ConstraintSet, bool> Ret;
  bool Fixed = false;
  unsigned LocalIter = 1;
  while (!Fixed) {
    auto FileName = BEFORE_SOLVING_SUFFIX + std::to_string(IterID) +
                       "_" + std::to_string(LocalIter);
    dumpConstraintOutputJson(FileName, Info);
    Ret = CS.solve(NumIter);
    if (NumIter > 1) {
      // This means we have made some changes to the environment
      // see if the function subtype handling causes any changes?
      Fixed = !Info.handleFunctionSubtyping();
      FileName = AFTER_SUBTYPING_SUFFIX + std::to_string(IterID) +
                    "_" + std::to_string(LocalIter);
      dumpConstraintOutputJson(FileName, Info);
    }
    else {
      // We reached a fixed point.
      Fixed = true;
    }
    LocalIter++;
  }
  return Ret;
}

bool performIterativeItypeRefinement(Constraints &CS, ProgramInfo &Info,
                                     std::set<std::string> &SourceFiles) {
  bool Fixed = false;
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

  dumpConstraintOutputJson(INITIAL_OUTPUT_SUFFIX, Info);

  while (!Fixed) {
    clock_t StartTime = clock();
    if (Verbose) {
      errs() << "****Iteration " << IterNum << " starts.****\n";
      errs() << "Iterative Itype refinement, Round:" << IterNum << "\n";
    }

    std::pair<Constraints::ConstraintSet, bool> R =
        solveConstraintsWithFunctionSubTyping(Info, IterNum);

    if (Verbose) {
      errs() << "Iteration:" << IterNum
             << ", Constraint solve time:" <<
                getTimeSpentInSeconds(StartTime) << "\n";
    }

    if (R.second) {
      if (Verbose) {
        errs() << "Constraints solved for iteration:" << IterNum << "\n";
      }
    }

    if (DumpStats) {
      Info.print_stats(SourceFiles, llvm::errs(), true);
    }

    // Get all the functions whose constraints have been modified.
    identifyModifiedFunctions(CS, ModFunctions);

    StartTime = clock();
    // Detect and update new found itype vars.
    NumItypeVars = detectAndUpdateITypeVars(Info, ModFunctions);

    if (Verbose) {
      errs() << "Iteration:" << IterNum
             <<
                ", Number of detected itype vars:" << NumItypeVars
             <<
                ", detection time:" << getTimeSpentInSeconds(StartTime) << "\n";
    }

    StartTime = clock();
    // Update the constraint graph by removing edges from/to iype parameters
    // and returns.
    EdgesRemoved = resetWithitypeConstraints(CS);

    if (Verbose) {
      errs() << "Iteration:" << IterNum
             << ", Number of edges removed:" << EdgesRemoved << "\n";

      errs() << "Iteration:" << IterNum
             << ", Refinement Time:" <<
                getTimeSpentInSeconds(StartTime) << "\n";
    }

    // If we removed any edges, that means we did not reach fix point.
    // In other words, we reach fixed point when no edges are removed from
    // the constraint graph.
    Fixed = !(EdgesRemoved > 0);
    if (Verbose) {
      errs() << "****Iteration " << IterNum << " ends****\n";
    }
    IterNum++;
  }
  if (Verbose) {
    errs() << "Fixed point reached after " << (IterNum - 1) <<
              " iterations.\n";
  }

  return Fixed;
}

std::set<std::string> FilePaths;

static CompilationDatabase *CurrCompDB = nullptr;

static tooling::CommandLineArguments SourceFiles;

bool CConvInterface::InitializeCConvert(CommonOptionsParser &OptionsParser,
                                        struct CConvertOptions &CCopt) {

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  DumpIntermediate = CCopt.DumpIntermediate;

  Verbose = CCopt.Verbose;

  SeperateMultipleFuncDecls = CCopt.SeperateMultipleFuncDecls;

  OutputPostfix = CCopt.OutputPostfix;

  ConstraintOutputJson = CCopt.ConstraintOutputJson;

  DumpStats = CCopt.DumpStats;

  HandleVARARGS = CCopt.HandleVARARGS;

  EnablePropThruIType = CCopt.EnablePropThruIType;

  ConsiderAllocUnsafe = CCopt.ConsiderAllocUnsafe;

  BaseDir = CCopt.BaseDir;

  // Get the absolute path of the base directory.
  std::string TmpPath = BaseDir;
  getAbsoluteFilePath(BaseDir, TmpPath);
  BaseDir = TmpPath;

  AllTypes = false;
  AddCheckedRegions = false;

  if (BaseDir.empty()) {
    SmallString<256>  cp;
    if (std::error_code ec = sys::fs::current_path(cp)) {
      errs() << "could not get current working dir\n";
      return false;
    }

    BaseDir = cp.str();
  }

  SourceFiles = OptionsParser.getSourcePathList();

  for (const auto &S : SourceFiles) {
    std::string AbsPath;
    if (getAbsoluteFilePath(S, AbsPath))
      FilePaths.insert(AbsPath);
  }

  CurrCompDB = &(OptionsParser.getCompilations());

  if (OutputPostfix == "-" && FilePaths.size() > 1) {
    errs() << "If rewriting more than one , can't output to stdout\n";
    return false;
  }
  return true;
}

bool CConvInterface::WriteConvertedFileToDisk(const std::string &FilePath) {
  if (std::find(SourceFiles.begin(), SourceFiles.end(), FilePath) !=
      SourceFiles.end()) {
    std::vector<std::string> SourceFiles;
    SourceFiles.clear();
    SourceFiles.push_back(FilePath);
    ClangTool Tool(*CurrCompDB, SourceFiles);
    std::unique_ptr<ToolAction> RewriteTool =
        newFrontendActionFactoryA<RewriteAction<RewriteConsumer,
                                                ProgramInfo>>(GlobalProgramInfo);

    if (RewriteTool)
      Tool.run(RewriteTool.get());
    return true;
  }
  return false;

}

bool CConvInterface::BuildInitialConstraints() {

  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  ClangTool Tool(*CurrCompDB, SourceFiles);

  // 1. Gather constraints.
  std::unique_ptr<ToolAction> ConstraintTool = newFrontendActionFactoryA<
      GenericAction<ConstraintBuilderConsumer,
                    ProgramInfo>>(GlobalProgramInfo);

  if (ConstraintTool)
    Tool.run(ConstraintTool.get());
  else
    llvm_unreachable("No action");

  if (!GlobalProgramInfo.link()) {
    errs() << "Linking failed!\n";
    return false;
  }

  // 2. Solve constraints.
  if (Verbose)
    outs() << "Solving constraints\n";

  Constraints &CS = GlobalProgramInfo.getConstraints();

  // perform constraint solving by iteratively refining based on itypes.
  bool Fixed = performIterativeItypeRefinement(CS,
                                               GlobalProgramInfo,
                                               FilePaths);

  assert(Fixed);
  if (Verbose)
    outs() << "Constraints solved\n";

  GlobalProgramInfo.computePointerDisjointSet();
  if (DumpIntermediate) {
    //Info.dump();
    dumpConstraintOutputJson(FINAL_OUTPUT_SUFFIX, GlobalProgramInfo);
  }
  return true;
}

#ifdef CCCONVSTANDALONE
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

  // Get the absolute path of the base directory.
  std::string TmpPath = BaseDir;
  getAbsoluteFilePath(BaseDir, TmpPath);
  BaseDir = TmpPath;

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

  // Perform constraint solving by iteratively refining based on itypes.
  bool Fixed = performIterativeItypeRefinement(CS,
                                               Info, InoutPaths);

  assert(Fixed == true);
  if (Verbose)
    outs() << "Constraints solved\n";
  if (DumpIntermediate) {
    Info.dump();
    dumpConstraintOutputJson(FINAL_OUTPUT_SUFFIX, Info);
  }

  // 3. Gather pre-rewrite data.
  std::unique_ptr<ToolAction> GatherTool =
    newFrontendActionFactoryA
    <RewriteAction<ArgGatherer, ProgramInfo>>(Info);
  if (GatherTool)
    Tool.run(GatherTool.get());
  else
    llvm_unreachable("No Action");

  unsigned NumOfRewrites = Info.MultipleRewrites ? 2 : 1;

  while (NumOfRewrites > 0) {
    // 4. Re-write based on constraints.
    std::unique_ptr<ToolAction> RewriteTool =
       newFrontendActionFactoryA
         <RewriteAction<RewriteConsumer, ProgramInfo>>(Info);
    if (RewriteTool)
      Tool.run(RewriteTool.get());
    else
      llvm_unreachable("No action");
    NumOfRewrites--;
  }

  if (DumpStats)
    Info.dump_stats(InoutPaths);

  return 0;
}
#endif
