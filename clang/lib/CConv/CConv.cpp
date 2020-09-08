//=--CConv.cpp----------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of various method in CConv.h
//
//===----------------------------------------------------------------------===//

#include "clang/CConv/CConv.h"
#include "clang/CConv/ConstraintBuilder.h"
#include "clang/CConv/IntermediateToolHook.h"
#include "clang/CConv/RewriteUtils.h"
#include "clang/Tooling/ArgumentsAdjusters.h"

#include "llvm/Support/TargetSelect.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;


// Suffixes for constraint output files.ParameterGatherer
#define INITIAL_OUTPUT_SUFFIX "_initial_constraints"
#define FINAL_OUTPUT_SUFFIX "_final_output"
#define BEFORE_SOLVING_SUFFIX "_before_solving_"
#define AFTER_SUBTYPING_SUFFIX "_after_subtyping_"

static cl::OptionCategory ArrBoundsInferCat("Array bounds inference options");
static cl::opt<bool> DebugArrSolver("debug-arr-solver",
                                   cl::desc("Dump array bounds inference graph"),
                                   cl::init(false),
                                    cl::cat(ArrBoundsInferCat));

bool DumpIntermediate;
bool Verbose;
std::string OutputPostfix;
std::string ConstraintOutputJson;
std::vector<std::string> AllocatorFunctions;
bool DumpStats;
bool HandleVARARGS;
bool EnablePropThruIType;
bool ConsiderAllocUnsafe;
std::string StatsOutputJson;
std::string WildPtrInfoJson;
bool AllTypes;
std::string BaseDir;
bool AddCheckedRegions;
bool DisableCCTypeChecker;
std::set<std::string> FilePaths;

static ClangTool *GlobalCTool = nullptr;

static CompilationDatabase *CurrCompDB = nullptr;
static tooling::CommandLineArguments SourceFiles;

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
        (new T(Info, OutputPostfix));
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

ArgumentsAdjuster getIgnoreCheckedPointerAdjuster() {
  return [](const CommandLineArguments &Args, StringRef /*unused*/) {
    CommandLineArguments AdjustedArgs;
    bool HasAdjuster = false;
    for (size_t i = 0, e = Args.size(); i < e; ++i) {
      StringRef Arg = Args[i];
      AdjustedArgs.push_back(Args[i]);
      if (Arg == "-fcheckedc-convert-tool") {
        HasAdjuster = true;
        break;
      }
    }
    if (!DisableCCTypeChecker && !HasAdjuster)
      AdjustedArgs.push_back("-fcheckedc-convert-tool");
    return AdjustedArgs;
  };
}

static ClangTool &getGlobalClangTool() {
  if (GlobalCTool == nullptr) {
    GlobalCTool = new ClangTool(*CurrCompDB, SourceFiles);
    GlobalCTool->appendArgumentsAdjuster(getIgnoreCheckedPointerAdjuster());
  }
  return *GlobalCTool;
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

void runSolver(ProgramInfo &Info,
               std::set<std::string> &SourceFiles) {
  Constraints &CS = Info.getConstraints();

  if (Verbose) {
    errs() << "Trying to capture Constraint Variables for all functions\n";
  }

  // Sanity check.
  assert(CS.checkInitialEnvSanity() && "Invalid initial environment. ");

  dumpConstraintOutputJson(INITIAL_OUTPUT_SUFFIX, Info);

  clock_t StartTime = clock();
  // FIXME: We should be seeing whether the constraints could be solved
  std::pair<Constraints::ConstraintSet, bool> R = CS.solve();
  if (Verbose) {
    errs() << "Solver time:" << getTimeSpentInSeconds(StartTime) << "\n";
  }
}

CConvInterface::CConvInterface(const struct CConvertOptions &CCopt,
                               const std::vector<std::string> &SourceFileList,
                               CompilationDatabase *CompDB) {

  DumpIntermediate = CCopt.DumpIntermediate;
  Verbose = CCopt.Verbose;
  OutputPostfix = CCopt.OutputPostfix;
  ConstraintOutputJson = CCopt.ConstraintOutputJson;
  StatsOutputJson = CCopt.StatsOutputJson;
  WildPtrInfoJson = CCopt.WildPtrInfoJson;
  DumpStats = CCopt.DumpStats;
  HandleVARARGS = CCopt.HandleVARARGS;
  EnablePropThruIType = CCopt.EnablePropThruIType;
  BaseDir = CCopt.BaseDir;
  AllTypes = CCopt.EnableAllTypes;
  AddCheckedRegions = CCopt.AddCheckedRegions;
  DisableCCTypeChecker = CCopt.DisableCCTypeChecker;
  AllocatorFunctions = CCopt.AllocatorFunctions;

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  ConstraintsBuilt = false;

  // Get the absolute path of the base directory.
  std::string TmpPath = BaseDir;
  getAbsoluteFilePath(BaseDir, TmpPath);
  BaseDir = TmpPath;

  if (BaseDir.empty()) {
    SmallString<256>  cp;
    if (std::error_code ec = sys::fs::current_path(cp)) {
      errs() << "could not get current working dir\n";
      assert(false && "Unable to get determine working directory.");
    }

    BaseDir = cp.str();
  }

  SourceFiles = SourceFileList;

  for (const auto &S : SourceFiles) {
    std::string AbsPath;
    if (getAbsoluteFilePath(S, AbsPath))
      FilePaths.insert(AbsPath);
  }

  CurrCompDB = CompDB;

  if (OutputPostfix == "-" && FilePaths.size() > 1) {
    errs() << "If rewriting more than one , can't output to stdout\n";
    assert(false && "Rewriting more than one files requires OutputPostfix");
  }
}

bool CConvInterface::BuildInitialConstraints() {

  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  ClangTool &Tool = getGlobalClangTool();

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

  ConstraintsBuilt = true;

  return true;
}

bool CConvInterface::SolveConstraints(bool ComputeInterimState) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
  assert(ConstraintsBuilt && "Constraints not yet built. We need to call "
                             "build constraint before trying to solve them." );
  // 2. Solve constraints.
  if (Verbose)
    outs() << "Solving constraints\n";

  if (DumpIntermediate)
    GlobalProgramInfo.dump();

  runSolver(GlobalProgramInfo, FilePaths);

  if (Verbose)
    outs() << "Constraints solved\n";

  if (ComputeInterimState)
    GlobalProgramInfo.computeInterimConstraintState(FilePaths);

  if (DumpIntermediate)
    dumpConstraintOutputJson(FINAL_OUTPUT_SUFFIX, GlobalProgramInfo);

  if (AllTypes) {
    if (DebugArrSolver)
      GlobalProgramInfo.getABoundsInfo().dumpAVarGraph(
          "arr_bounds_initial.dot");

    // Propagate initial data-flow information for Array pointers.
    GlobalProgramInfo.getABoundsInfo().performFlowAnalysis(&GlobalProgramInfo);
  }

  // 3. Run intermediate tool hook to run visitors that need to be executed
  // after constraint solving but before rewriting.
  ClangTool &Tool = getGlobalClangTool();
  std::unique_ptr<ToolAction> IMTool =
      newFrontendActionFactoryA
          <GenericAction<IntermediateToolHook, ProgramInfo>>(GlobalProgramInfo);
  if (IMTool)
    Tool.run(IMTool.get());
  else
    llvm_unreachable("No Action");

  if (AllTypes) {
    // Propagate data-flow information for Array pointers.
    GlobalProgramInfo.getABoundsInfo().performFlowAnalysis(&GlobalProgramInfo);

    if (DebugArrSolver)
      GlobalProgramInfo.getABoundsInfo().dumpAVarGraph("arr_bounds_final.dot");
  }

  if (DumpStats) {
    GlobalProgramInfo.print_stats(FilePaths, llvm::errs(), true);
    std::error_code Ec;
    llvm::raw_fd_ostream OutputJson(StatsOutputJson, Ec);
    if (!OutputJson.has_error()) {
      GlobalProgramInfo.print_stats(FilePaths, OutputJson, false, true);
      OutputJson.close();
    }

    llvm::raw_fd_ostream WildPtrInfo(WildPtrInfoJson, Ec);
    if (!WildPtrInfo.has_error()) {
      GlobalProgramInfo.computeInterimConstraintState(FilePaths);
      GlobalProgramInfo.getInterimConstraintState().print_stats(WildPtrInfo);
      WildPtrInfo.close();
    }
  }

  return true;
}

bool CConvInterface::WriteConvertedFileToDisk(const std::string &FilePath) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
  if (std::find(SourceFiles.begin(), SourceFiles.end(), FilePath) !=
      SourceFiles.end()) {
    std::vector<std::string> SourceFiles;
    SourceFiles.clear();
    SourceFiles.push_back(FilePath);
    // Don't use global tool. Create a new tool for give single file.
    ClangTool Tool(*CurrCompDB, SourceFiles);
    Tool.appendArgumentsAdjuster(getIgnoreCheckedPointerAdjuster());
    std::unique_ptr<ToolAction> RewriteTool =
        newFrontendActionFactoryA<RewriteAction<RewriteConsumer,
    ProgramInfo>>(GlobalProgramInfo);

    if (RewriteTool)
      Tool.run(RewriteTool.get());
    return true;
  }
  return false;

}

bool CConvInterface::WriteAllConvertedFilesToDisk() {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  ClangTool &Tool = getGlobalClangTool();

  // Rewrite the input files
  std::unique_ptr<ToolAction> RewriteTool =
      newFrontendActionFactoryA<
          RewriteAction<RewriteConsumer, ProgramInfo>>(GlobalProgramInfo);
  if (RewriteTool)
    Tool.run(RewriteTool.get());
  else
    llvm_unreachable("No action");

  return true;
}

ConstraintsInfo &CConvInterface::GetWILDPtrsInfo() {
  return GlobalProgramInfo.getInterimConstraintState();
}

bool CConvInterface::MakeSinglePtrNonWild(ConstraintKey targetPtr) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
  CVars RemovePtrs;
  RemovePtrs.clear();

  auto &PtrDisjointSet = GlobalProgramInfo.getInterimConstraintState();
  auto &CS = GlobalProgramInfo.getConstraints();

  // Get all the current WILD pointers.
  CVars OldWildPtrs = PtrDisjointSet.AllWildPtrs;

  // Delete the constraint that make the provided targetPtr WILD.
  VarAtom *VA = CS.getOrCreateVar(targetPtr, "q", VarAtom::V_Other);
  Geq newE(VA, CS.getWild());
  Constraint *originalConstraint = *CS.getConstraints().find(&newE);
  CS.removeConstraint(originalConstraint);
  VA->getAllConstraints().erase(originalConstraint);
  delete(originalConstraint);

  // Reset the constraint system.
  CS.resetEnvironment();

  // Solve the constraints.
  //assert (CS == GlobalProgramInfo.getConstraints());
  runSolver(GlobalProgramInfo, FilePaths);

  // Compute new disjoint set.
  GlobalProgramInfo.computeInterimConstraintState(FilePaths);

  // Get new WILD pointers.
  CVars &NewWildPtrs = PtrDisjointSet.AllWildPtrs;

  // Get the number of pointers that have now converted to non-WILD.
  std::set_difference(OldWildPtrs.begin(), OldWildPtrs.end(),
                      NewWildPtrs.begin(), NewWildPtrs.end(),
                      std::inserter(RemovePtrs, RemovePtrs.begin()));

  return !RemovePtrs.empty();
}


void CConvInterface::InvalidateAllConstraintsWithReason(
    Constraint *ConstraintToRemove) {
  // Get the reason for the current constraint.
  std::string ConstraintRsn = ConstraintToRemove->getReason();
  Constraints::ConstraintSet ToRemoveConstraints;
  Constraints &CS = GlobalProgramInfo.getConstraints();
  // Remove all constraints that have the reason.
  CS.removeAllConstraintsOnReason(ConstraintRsn, ToRemoveConstraints);

  // Free up memory by deleting all the removed constraints.
  for (auto *toDelCons : ToRemoveConstraints) {
    assert(dyn_cast<Geq>(toDelCons) && "We can only delete Geq constraints.");
    Geq*TCons = dyn_cast<Geq>(toDelCons);
    auto *Vatom = dyn_cast<VarAtom>(TCons->getLHS());
    assert(Vatom != nullptr && "Equality constraint with out VarAtom as LHS");
    VarAtom *VS = CS.getOrCreateVar(Vatom->getLoc(), "q", VarAtom::V_Other);
    VS->getAllConstraints().erase(TCons);
    delete (toDelCons);
  }
}

bool CConvInterface::InvalidateWildReasonGlobally(ConstraintKey PtrKey) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  CVars RemovePtrs;
  RemovePtrs.clear();

  auto &PtrDisjointSet = GlobalProgramInfo.getInterimConstraintState();
  auto &CS = GlobalProgramInfo.getConstraints();

  CVars OldWildPtrs = PtrDisjointSet.AllWildPtrs;

  // Delete ALL the constraints that have the same given reason.
  VarAtom *VA = CS.getOrCreateVar(PtrKey, "q", VarAtom::V_Other);
  Geq NewE(VA, CS.getWild());
  Constraint *OriginalConstraint = *CS.getConstraints().find(&NewE);
  InvalidateAllConstraintsWithReason(OriginalConstraint);

  // Reset constraint solver.
  CS.resetEnvironment();

  // Solve the constraints.
  runSolver(GlobalProgramInfo, FilePaths);

  // Recompute the WILD pointer disjoint sets.
  GlobalProgramInfo.computeInterimConstraintState(FilePaths);

  // Computed the number of removed pointers.
  CVars &NewWildPtrs = PtrDisjointSet.AllWildPtrs;

  std::set_difference(OldWildPtrs.begin(), OldWildPtrs.end(),
                      NewWildPtrs.begin(), NewWildPtrs.end(),
                      std::inserter(RemovePtrs, RemovePtrs.begin()));

  return !RemovePtrs.empty();
}
