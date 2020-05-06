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
#include "clang/CConv/IterativeItypeHelper.h"
#include "clang/CConv/RewriteUtils.h"

#include "llvm/Support/TargetSelect.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;


// Suffixes for constraint output files.
#define INITIAL_OUTPUT_SUFFIX "_initial_constraints"
#define FINAL_OUTPUT_SUFFIX "_final_output"
#define BEFORE_SOLVING_SUFFIX "_before_solving_"
#define AFTER_SUBTYPING_SUFFIX "_after_subtyping_"

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

std::set<std::string> FilePaths;

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
                                      unsigned long IterID) {
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

void performIterativeItypeRefinement(ProgramInfo &Info,
                                     std::set<std::string> &SourceFiles) {
  unsigned long IterNum = 1;
  unsigned long EdgesRemoved = 0;
  unsigned long NumItypeVars = 0;
  std::set<ItypeModFuncsKType> ModFunctions;
  Constraints &CS = Info.getConstraints();

  if (Verbose) {
    errs() << "Trying to capture Constraint Variables for all functions\n";
  }
  // First capture itype parameters and return values for all functions.
  performConstraintSetup(Info);

  // Sanity check.
  assert(CS.checkInitialEnvSanity() && "Invalid initial environment. ");

  dumpConstraintOutputJson(INITIAL_OUTPUT_SUFFIX, Info);

  do {

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

    if (Verbose) {
      errs() << "****Iteration " << IterNum << " ends****\n";
    }
    IterNum++;
    // If we removed any edges, that means we did not reach fix point.
    // In other words, we reach fixed point when no edges are removed from
    // the constraint graph.
  } while (EdgesRemoved > 0);

  if (Verbose) {
    errs() << "Fixed point reached after " << (IterNum - 1) <<
           " iterations.\n";
  }
}

CConvInterface::CConvInterface(const struct CConvertOptions &CCopt,
                               const std::vector<std::string> &SourceFileList,
                               CompilationDatabase *CompDB) {

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

  ConstraintsBuilt = false;

  // Get the absolute path of the base directory.
  std::string TmpPath = BaseDir;
  getAbsoluteFilePath(BaseDir, TmpPath);
  BaseDir = TmpPath;

  AllTypes = CCopt.EnableAllTypes;
  AddCheckedRegions = CCopt.AddCheckedRegions;

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

  ConstraintsBuilt = true;

  return true;
}

bool CConvInterface::SolveConstraints() {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
  assert(ConstraintsBuilt && "Constraints not yet built. We need to call "
                             "build constraint before trying to solve them." );
  // 2. Solve constraints.
  if (Verbose)
    outs() << "Solving constraints\n";

  if (DumpIntermediate) {
    GlobalProgramInfo.dump();
  }

  // perform constraint solving by iteratively refining based on itypes.
  performIterativeItypeRefinement(GlobalProgramInfo,FilePaths);

  if (Verbose)
    outs() << "Constraints solved\n";

  GlobalProgramInfo.computePointerDisjointSet();
  if (DumpIntermediate) {
    dumpConstraintOutputJson(FINAL_OUTPUT_SUFFIX, GlobalProgramInfo);
  }

  // 3. Gather pre-rewrite data.
  ClangTool Tool(*CurrCompDB, SourceFiles);
  std::unique_ptr<ToolAction> GatherTool =
      newFrontendActionFactoryA
          <RewriteAction<ArgGatherer, ProgramInfo>>(GlobalProgramInfo);
  if (GatherTool)
    Tool.run(GatherTool.get());
  else
    llvm_unreachable("No Action");

  return true;
}

bool CConvInterface::WriteConvertedFileToDisk(const std::string &FilePath) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
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

bool CConvInterface::WriteAllConvertedFilesToDisk() {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
  unsigned NumOfRewrites = GlobalProgramInfo.MultipleRewrites ? 2 : 1;
  ClangTool Tool(*CurrCompDB, SourceFiles);
  while (NumOfRewrites > 0) {
    // 4. Re-write based on constraints.
    std::unique_ptr<ToolAction> RewriteTool =
        newFrontendActionFactoryA<
            RewriteAction<RewriteConsumer, ProgramInfo>>(GlobalProgramInfo);

    if (RewriteTool)
      Tool.run(RewriteTool.get());
    else
      llvm_unreachable("No action");
    NumOfRewrites--;
  }

  if (DumpStats)
    GlobalProgramInfo.dump_stats(FilePaths);

  return true;
}

void CConvInterface::ResetAllPointerConstraints() {
  // Restore all the deleted constraints.
  Constraints::EnvironmentMap &EnvMap =
      GlobalProgramInfo.getConstraints().getVariables();
  for (auto &CV : EnvMap) {
    CV.first->resetErasedConstraints();
  }
}

DisjointSet &CConvInterface::GetWILDPtrsInfo() {
  return GlobalProgramInfo.getPointerConstraintDisjointSet();
}

bool CConvInterface::MakeSinglePtrNonWild(ConstraintKey targetPtr) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
  CVars RemovePtrs;
  RemovePtrs.clear();

  auto &PtrDisjointSet = GlobalProgramInfo.getPointerConstraintDisjointSet();
  auto &CS = GlobalProgramInfo.getConstraints();

  // Get all the current WILD pointers.
  CVars OldWildPtrs = PtrDisjointSet.AllWildPtrs;

  // Reset all the pointer constraints.
  ResetAllPointerConstraints();

  // Delete the constraint that make the provided targetPtr WILD.
  VarAtom *VA = CS.getOrCreateVar(targetPtr);
  Geq newE(VA, CS.getWild());
  Constraint *originalConstraint = *CS.getConstraints().find(&newE);
  CS.removeConstraint(originalConstraint);
  VA->getAllConstraints().erase(originalConstraint);
  delete(originalConstraint);

  // Reset the constraint system.
  CS.resetConstraints();

  // Solve the constraints.
  //assert (CS == GlobalProgramInfo.getConstraints());
  performIterativeItypeRefinement(GlobalProgramInfo, FilePaths);

  // Compute new disjoint set.
  GlobalProgramInfo.computePointerDisjointSet();

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
    assert(dyn_cast<Eq>(toDelCons) && "We can only delete Eq constraints.");
    Eq*TCons = dyn_cast<Eq>(toDelCons);
    auto *Vatom = dyn_cast<VarAtom>(TCons->getLHS());
    assert(Vatom != nullptr && "Equality constraint with out VarAtom as LHS");
    VarAtom *VS = CS.getOrCreateVar(Vatom->getLoc());
    VS->getAllConstraints().erase(TCons);
    delete (toDelCons);
  }
}

bool CConvInterface::InvalidateWildReasonGlobally(ConstraintKey PtrKey) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  CVars RemovePtrs;
  RemovePtrs.clear();

  auto &PtrDisjointSet = GlobalProgramInfo.getPointerConstraintDisjointSet();
  auto &CS = GlobalProgramInfo.getConstraints();

  CVars OldWildPtrs = PtrDisjointSet.AllWildPtrs;

  ResetAllPointerConstraints();

  // Delete ALL the constraints that have the same given reason.
  VarAtom *VA = CS.getOrCreateVar(PtrKey);
  Geq NewE(VA, CS.getWild());
  Constraint *OriginalConstraint = *CS.getConstraints().find(&NewE);
  InvalidateAllConstraintsWithReason(OriginalConstraint);

  // Reset constraint solver.
  CS.resetConstraints();

  // Solve the constraint.
  //assert(CS == GlobalProgramInfo.getConstraints());
  performIterativeItypeRefinement(GlobalProgramInfo, FilePaths);

  // Recompute the WILD pointer disjoint sets.
  GlobalProgramInfo.computePointerDisjointSet();

  // Computed the number of removed pointers.
  CVars &NewWildPtrs = PtrDisjointSet.AllWildPtrs;

  std::set_difference(OldWildPtrs.begin(), OldWildPtrs.end(),
                      NewWildPtrs.begin(), NewWildPtrs.end(),
                      std::inserter(RemovePtrs, RemovePtrs.begin()));

  return !RemovePtrs.empty();
}
