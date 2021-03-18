//=--ProgramInfo.cpp----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of ProgramInfo methods.
//===----------------------------------------------------------------------===//

#include "clang/3C/ProgramInfo.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ConstraintsGraph.h"
#include "clang/3C/MappingVisitor.h"
#include "clang/3C/Utils.h"
#include <sstream>

using namespace clang;

void PerformanceStats::startCompileTime() {
  CompileTimeSt = clock();
}

void PerformanceStats::endCompileTime() {
  CompileTime += getTimeSpentInSeconds(CompileTimeSt);
}

void PerformanceStats::startConstraintBuilderTime() {
  ConstraintBuilderTimeSt = clock();
}

void PerformanceStats::endConstraintBuilderTime() {
  ConstraintBuilderTime += getTimeSpentInSeconds(ConstraintBuilderTimeSt);
}

void PerformanceStats::startConstraintSolverTime() {
  ConstraintSolverTimeSt = clock();
}

void PerformanceStats::endConstraintSolverTime() {
  ConstraintSolverTime += getTimeSpentInSeconds(ConstraintSolverTimeSt);
}

void PerformanceStats::startArrayBoundsInferenceTime() {
  ArrayBoundsInferenceTimeSt = clock();
}

void PerformanceStats::endArrayBoundsInferenceTime() {
  ArrayBoundsInferenceTime += getTimeSpentInSeconds(ArrayBoundsInferenceTimeSt);
}

void PerformanceStats::startRewritingTime() {
  RewritingTimeSt = clock();
}

void PerformanceStats::endRewritingTime() {
  RewritingTime += getTimeSpentInSeconds(RewritingTimeSt);
}

void PerformanceStats::startTotalTime() {
  TotalTimeSt = clock();
}

void PerformanceStats::endTotalTime() {
  TotalTime += getTimeSpentInSeconds(TotalTimeSt);
}

void PerformanceStats::printPerformanceStats(raw_ostream &O) {
  O << "{\"TotalTime\":" << TotalTime;
  O << ", \"ConstraintBuilderTime\":" << ConstraintBuilderTime;
  O << ", \"ConstraintSolverTime\":" << ConstraintSolverTime;
  O << ", \"ArrayBoundsInferenceTime\":" << ArrayBoundsInferenceTime;
  O << ", \"RewritingTime\":" << RewritingTime;
  O << "}";
}

ProgramInfo::ProgramInfo() : Persisted(true) {
  ExternalFunctionFVCons.clear();
  StaticFunctionFVCons.clear();
}

void dumpExtFuncMap(const ProgramInfo::ExternalFunctionMapType &EMap,
                    raw_ostream &O) {
  for (const auto &DefM : EMap) {
    O << "Func Name:" << DefM.first << " => [ ";
    DefM.second->print(O);
    O << " ]\n";
  }
}

void dumpStaticFuncMap(const ProgramInfo::StaticFunctionMapType &EMap,
                       raw_ostream &O) {
  for (const auto &DefM : EMap) {
    O << "File Name:" << DefM.first << " => ";
    for (const auto &Tmp : DefM.second) {
      O << " Func Name:" << Tmp.first << " => [ \n";
      Tmp.second->print(O);
      O << " ]\n";
    }
    O << "\n";
  }
}

void dumpExtFuncMapJson(const ProgramInfo::ExternalFunctionMapType &EMap,
                        raw_ostream &O) {
  bool AddComma = false;
  for (const auto &DefM : EMap) {
    if (AddComma) {
      O << ",\n";
    }
    O << "{\"FuncName\":\"" << DefM.first << "\", \"Constraints\":[";
    DefM.second->dumpJson(O);
    O << "]}";
    AddComma = true;
  }
}

void dumpStaticFuncMapJson(const ProgramInfo::StaticFunctionMapType &EMap,
                           raw_ostream &O) {
  bool AddComma = false;
  for (const auto &DefM : EMap) {
    if (AddComma) {
      O << ",\n";
    }
    O << "{\"FuncName\":\"" << DefM.first << "\", \"Constraints\":[";
    bool AddComma1 = false;
    for (const auto &J : DefM.second) {
      if (AddComma1) {
        O << ",";
      }
      O << "{\"FileName\":\"" << J.first << "\", \"FVConstraints\":[";
      J.second->dumpJson(O);
      O << "]}\n";
      AddComma1 = true;
    }
    O << "]}";
    AddComma = true;
  }
}

void ProgramInfo::print(raw_ostream &O) const {
  CS.print(O);
  O << "\n";

  O << "Constraint Variables\n";
  for (const auto &I : Variables) {
    PersistentSourceLoc L = I.first;
    L.print(O);
    O << "=>[ ";
    I.second->print(O);
    O << " ]\n";
  }

  O << "External Function Definitions\n";
  dumpExtFuncMap(ExternalFunctionFVCons, O);
  O << "Static Function Definitions\n";
  dumpStaticFuncMap(StaticFunctionFVCons, O);
}

void ProgramInfo::dumpJson(llvm::raw_ostream &O) const {
  O << "{\"Setup\":";
  CS.dumpJson(O);
  // Dump the constraint variables.
  O << ", \"ConstraintVariables\":[";
  bool AddComma = false;
  for (const auto &I : Variables) {
    if (AddComma) {
      O << ",\n";
    }
    PersistentSourceLoc L = I.first;

    O << "{\"line\":\"";
    L.print(O);
    O << "\",\"Variables\":[";
    I.second->dumpJson(O);
    O << "]}";
    AddComma = true;
  }
  O << "]";
  O << ", \"ExternalFunctionDefinitions\":[";
  dumpExtFuncMapJson(ExternalFunctionFVCons, O);
  O << "], \"StaticFunctionDefinitions\":[";
  dumpStaticFuncMapJson(StaticFunctionFVCons, O);
  O << "]}";
}

// Given a ConstraintVariable V, retrieve all of the unique
// constraint variables used by V. If V is just a
// PointerVariableConstraint, then this is just the contents
// of 'vars'. If it either has a function pointer, or V is
// a function, then recurses on the return and parameter
// constraints.
static void getVarsFromConstraint(ConstraintVariable *V, CAtoms &R) {
  if (auto *PVC = dyn_cast<PVConstraint>(V)) {
    R.insert(R.begin(), PVC->getCvars().begin(), PVC->getCvars().end());
    if (FVConstraint *FVC = PVC->getFV())
      getVarsFromConstraint(FVC, R);
  } else if (auto *FVC = dyn_cast<FVConstraint>(V)) {
    if (FVC->getExternalReturn())
      getVarsFromConstraint(FVC->getExternalReturn(), R);
    for (unsigned I = 0; I < FVC->numParams(); I++)
      getVarsFromConstraint(FVC->getExternalParam(I), R);
  }
}

// Print aggregate stats
void ProgramInfo::print_aggregate_stats(const std::set<std::string> &F,
                                        llvm::raw_ostream &O) {
  std::set<Atom *> AllAtoms;
  CAtoms FoundVars;

  unsigned int totP, totNt, totA, totWi;
  totP = totNt = totA = totWi = 0;

  CVarSet ArrPtrs, NtArrPtrs;
  ConstraintVariable *Tmp = nullptr;

  for (auto &I : Variables) {
    ConstraintVariable *C = I.second;
    std::string FileName = I.first.getFileName();
    if (F.count(FileName) || FileName.find(BaseDir) != std::string::npos) {
      if (C->isForValidDecl()) {
        FoundVars.clear();
        getVarsFromConstraint(C, FoundVars);
        AllAtoms.insert(FoundVars.begin(), FoundVars.end());
        Tmp = C;
        if (FVConstraint *FV = dyn_cast<FVConstraint>(C)) {
          Tmp = FV->getExternalReturn();
        }
        // If this is a var atom?
        if (!FoundVars.empty() && dyn_cast_or_null<VarAtom>(*FoundVars.begin())) {
          if (Tmp->hasNtArr(CS.getVariables(), 0)) {
            NtArrPtrs.insert(Tmp);
          } else if (Tmp->hasArr(CS.getVariables(), 0)) {
            ArrPtrs.insert(Tmp);
          }
        }
      }
    }
  }

  for (const auto &N : AllAtoms) {
    ConstAtom *CA = CS.getAssignment(N);
    switch (CA->getKind()) {
      case Atom::A_Arr:
        totA += 1;
        break;
      case Atom::A_NTArr:
        totNt += 1;
        break;
      case Atom::A_Ptr:
        totP += 1;
        break;
      case Atom::A_Wild:
        totWi += 1;
        break;
      case Atom::A_Var:
      case Atom::A_Const:
        llvm_unreachable("bad constant in environment map");
    }
  }

  O << "{\"AggregateStats\":[";
  O << "{\"" << "TotalStats" << "\":{";
  O << "\"constraints\":" << AllAtoms.size() << ",";
  O << "\"ptr\":" << totP << ",";
  O << "\"ntarr\":" << totNt << ",";
  O << "\"arr\":" << totA << ",";
  O << "\"wild\":" << totWi;
  O << "}},";
  O << "{\"ArrBoundsStats\":";
  ArrBInfo.printStats(O, ArrPtrs, true);
  O << "},";
  O << "{\"NtArrBoundsStats\":";
  ArrBInfo.printStats(O, NtArrPtrs, true);
  O << "},";
  O << "{\"TimingStats\":";
  PerfS.printPerformanceStats(O);
  O << "}";
  O<<"]}";

}

// Print out statistics of constraint variables on a per-file basis.
void ProgramInfo::printStats(const std::set<std::string> &F, raw_ostream &O,
                             bool OnlySummary, bool JsonFormat) {
  if (!OnlySummary && !JsonFormat) {
    O << "Enable itype propagation:" << EnablePropThruIType << "\n";
    O << "Sound handling of var args functions:" << HandleVARARGS << "\n";
  }
  std::map<std::string, std::tuple<int, int, int, int, int>> FilesToVars;
  CVarSet InSrcCVars;
  unsigned int TotC, TotP, TotNt, TotA, TotWi;
  TotC = TotP = TotNt = TotA = TotWi = 0;

  // First, build the map and perform the aggregation.
  for (auto &I : Variables) {
    std::string FileName = I.first.getFileName();
    if (F.count(FileName) || FileName.find(BaseDir) != std::string::npos) {
      int VarC = 0;
      int PC = 0;
      int NtaC = 0;
      int AC = 0;
      int WC = 0;

      auto J = FilesToVars.find(FileName);
      if (J != FilesToVars.end())
        std::tie(VarC, PC, NtaC, AC, WC) = J->second;

      ConstraintVariable *C = I.second;
      if (C->isForValidDecl()) {
        InSrcCVars.insert(C);
        CAtoms FoundVars;
        getVarsFromConstraint(C, FoundVars);

        VarC += FoundVars.size();
        for (const auto &N : FoundVars) {
          ConstAtom *CA = CS.getAssignment(N);
          switch (CA->getKind()) {
          case Atom::A_Arr:
            AC += 1;
            break;
          case Atom::A_NTArr:
            NtaC += 1;
            break;
          case Atom::A_Ptr:
            PC += 1;
            break;
          case Atom::A_Wild:
            WC += 1;
            break;
          case Atom::A_Var:
          case Atom::A_Const:
            llvm_unreachable("bad constant in environment map");
          }
        }
      }
      FilesToVars[FileName] =
          std::tuple<int, int, int, int, int>(VarC, PC, NtaC, AC, WC);
    }
  }

  // Then, dump the map to output.
  // if not only summary then dump everything.
  if (JsonFormat) {
    O << "{\"Stats\":{";
    O << "\"ConstraintStats\":{";
  }
  if (!OnlySummary) {
    if (JsonFormat) {
      O << "\"Individual\":[";
    } else {
      O << "file|#constraints|#ptr|#ntarr|#arr|#wild\n";
    }
  }
  bool AddComma = false;
  for (const auto &I : FilesToVars) {
    int V, P, Nt, A, W;
    std::tie(V, P, Nt, A, W) = I.second;

    TotC += V;
    TotP += P;
    TotNt += Nt;
    TotA += A;
    TotWi += W;
    if (!OnlySummary) {
      if (JsonFormat) {
        if (AddComma) {
          O << ",\n";
        }
        O << "{\"" << I.first << "\":{";
        O << "\"constraints\":" << V << ",";
        O << "\"ptr\":" << P << ",";
        O << "\"ntarr\":" << Nt << ",";
        O << "\"arr\":" << A << ",";
        O << "\"wild\":" << W;
        O << "}}";
        AddComma = true;
      } else {
        O << I.first << "|" << V << "|" << P << "|" << Nt << "|" << A << "|"
          << W;
        O << "\n";
      }
    }
  }
  if (!OnlySummary && JsonFormat) {
    O << "],";
  }

  if (!JsonFormat) {
    O << "Summary\nTotalConstraints|TotalPtrs|TotalNTArr|TotalArr|TotalWild\n";
    O << TotC << "|" << TotP << "|" << TotNt << "|" << TotA << "|" << TotWi
      << "\n";
  } else {
    O << "\"Summary\":{";
    O << "\"TotalConstraints\":" << TotC << ",";
    O << "\"TotalPtrs\":" << TotP << ",";
    O << "\"TotalNTArr\":" << TotNt << ",";
    O << "\"TotalArr\":" << TotA << ",";
    O << "\"TotalWild\":" << TotWi;
    O << "}},\n";
  }

  if (AllTypes) {
    if (JsonFormat) {
      O << "\"BoundsStats\":";
    }
    ArrBInfo.printStats(O, InSrcCVars, JsonFormat);
  }

  if (JsonFormat) {
    O << ",";
    O << "\"TimingStats\":";
    PerfS.printPerformanceStats(O);
    O << "}}";
  }
}

bool ProgramInfo::link() {
  // For every global symbol in all the global symbols that we have found
  // go through and apply rules for whether they are functions or variables.
  if (Verbose)
    llvm::errs() << "Linking!\n";

  // Equate the constraints for all global variables.
  // This is needed for variables that are defined as extern.
  for (const auto &V : GlobalVariableSymbols) {
    const std::set<PVConstraint *> &C = V.second;

    if (C.size() > 1) {
      std::set<PVConstraint *>::iterator I = C.begin();
      std::set<PVConstraint *>::iterator J = C.begin();
      ++J;
      if (Verbose)
        llvm::errs() << "Global variables:" << V.first << "\n";
      while (J != C.end()) {
        constrainConsVarGeq(*I, *J, CS, nullptr, Same_to_Same, true, this);
        ++I;
        ++J;
      }
    }
  }

  for (const auto &V : ExternGVars) {
    // if a definition for this global variable has not been seen,
    // constrain everything about it
    if (!V.second) {
      std::string VarName = V.first;
      std::string Rsn =
          "External global variable " + VarName + " has no definition";
      const std::set<PVConstraint *> &C = GlobalVariableSymbols[VarName];
      for (const auto &Var : C) {
        Var->constrainToWild(CS, Rsn);
      }
    }
  }

  // For every global function that is an unresolved external, constrain
  // its parameter types to be wild. Unless it has a bounds-safe annotation.
  for (const auto &U : ExternalFunctionFVCons) {
    std::string FuncName = U.first;
    FVConstraint *G = U.second;
    // If we've seen this symbol, but never seen a body for it, constrain
    // everything about it.
    // Some global symbols we don't need to constrain to wild, like
    // malloc and free. Check those here and skip if we find them.
    if (!G->hasBody()) {

      // If there was a checked type on a variable in the input program, it
      // should stay that way. Otherwise, we shouldn't be adding a checked type
      // to an extern function.
      std::string Rsn =
          "Unchecked pointer in parameter or return of external function " +
          FuncName;
      G->getInternalReturn()->constrainToWild(CS, Rsn);
      if (!G->getExternalReturn()->getIsGeneric())
        G->getExternalReturn()->constrainToWild(CS, Rsn);
      for (unsigned I = 0; I < G->numParams(); I++) {
        G->getInternalParam(I)->constrainToWild(CS, Rsn);
        if (!G->getExternalParam(I)->getIsGeneric())
          G->getExternalParam(I)->constrainToWild(CS, Rsn);
      }
    }
  }
  // Repeat for static functions.
  //
  // Static functions that don't have a body will always cause a linking
  // error during compilation. They may still be useful as code is developed,
  // so we treat them as if they are external, and constrain parameters
  // to wild as appropriate.
  for (const auto &U : StaticFunctionFVCons) {
    for (const auto &V : U.second) {

      std::string FileName = U.first;
      std::string FuncName = V.first;
      FVConstraint *G = V.second;
      if (!G->hasBody()) {

        std::string Rsn =
            "Unchecked pointer in parameter or return of static function " +
            FuncName + " in " + FileName;
        if (!G->getExternalReturn()->getIsGeneric())
          G->getExternalReturn()->constrainToWild(CS, Rsn);
        for (unsigned I = 0; I < G->numParams(); I++)
          if (!G->getExternalParam(I)->getIsGeneric())
            G->getExternalParam(I)->constrainToWild(CS, Rsn);
      }
    }
  }

  return true;
}

// Populate Variables, VarDeclToStatement, RVariables, and DepthMap with
// AST data structures that correspond do the data stored in PDMap and
// ReversePDMap.
void ProgramInfo::enterCompilationUnit(ASTContext &Context) {
  assert(Persisted);
  // Get a set of all of the PersistentSourceLoc's we need to fill in.
  std::set<PersistentSourceLoc> P;
  //for (auto I : PersistentVariables)
  //  P.insert(I.first);

  // Resolve the PersistentSourceLoc to one of Decl,Stmt,Type.
  MappingVisitor V(P, Context);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  for (const auto &D : TUD->decls())
    V.TraverseDecl(D);

  Persisted = false;
  return;
}

// Remove any references we maintain to AST data structure pointers.
// After this, the Variables, VarDeclToStatement, RVariables, and DepthMap
// should all be empty.
void ProgramInfo::exitCompilationUnit() {
  assert(!Persisted);
  Persisted = true;
  return;
}

FunctionVariableConstraint *
ProgramInfo::insertNewFVConstraint(FunctionDecl *FD, FVConstraint *NewC,
                                        ASTContext *C) {
  std::string FuncName = FD->getNameAsString();

  // Choose a storage location

  // assume a global function, but change to a static if not
  ExternalFunctionMapType *Map = &ExternalFunctionFVCons;
  if (!FD->isGlobal()) {
    // if the filename has not yet been seen, just insert and we're done
    auto Psl = PersistentSourceLoc::mkPSL(FD, *C);
    std::string FileName = Psl.getFileName();
    if (StaticFunctionFVCons.find(FileName) == StaticFunctionFVCons.end()){
      StaticFunctionFVCons[FileName][FuncName] = NewC;
      return NewC;
    }

    // store in static map
    Map = &StaticFunctionFVCons[FileName];
  }

  // if the function has not yet been seen, just insert and we're done
  if (Map->find(FuncName) == Map->end()) {
    (*Map)[FuncName] = NewC;
    return NewC;
  }

  // Resolve conflicts

  // We need to keep the version with a body, if it exists,
  // so branch based on it
  auto *OldC = (*Map)[FuncName];
  bool NewHasBody = NewC->hasBody();
  bool OldHasBody = OldC->hasBody();
  std::string ReasonFailed = "";

  if (OldHasBody && NewHasBody) {
    // Two separate bodies for a function is irreconcilable
    ReasonFailed = "multiple function bodies";
  } else if (OldHasBody && !NewHasBody) {
    OldC->mergeDeclaration(NewC, *this, ReasonFailed);
  } else if (!OldHasBody && NewHasBody) {
    NewC->mergeDeclaration(OldC, *this, ReasonFailed);
    (*Map)[FuncName] = NewC;
  } else if (!OldHasBody && !NewHasBody) {
    // lacking bodies, we favor declared params
    if (OldC->numParams() == 0 && NewC->numParams() != 0) {
      NewC->mergeDeclaration(OldC, *this, ReasonFailed);
      (*Map)[FuncName] = NewC;
    } else {
      OldC->mergeDeclaration(NewC, *this, ReasonFailed);
    }
  }

  // If successful, we're done and can skip error reporting
  if (ReasonFailed == "") return (*Map)[FuncName];

  // Error reporting
  { // block to force DiagBuilder destructor and emit message
    clang::DiagnosticsEngine &DE = C->getDiagnostics();
    unsigned FailID = DE.getCustomDiagID(DiagnosticsEngine::Fatal,
                                         "merging failed for %q0 due to %1");
    const auto Pointer = reinterpret_cast<intptr_t>(FD);
    const auto Kind = clang::DiagnosticsEngine::ArgumentKind::ak_nameddecl;
    auto DiagBuilder = DE.Report(FD->getLocation(), FailID);
    DiagBuilder.AddTaggedVal(Pointer, Kind);
    DiagBuilder.AddString(ReasonFailed);
  }
  // Kill the process and stop conversion
  // Without this code here, 3C simply ignores this pair of functions
  // and converts the rest of the files as it will (in semi-compliance
  // with Mike's (2) listed on the original issue (#283)
  exit(1);
}

void ProgramInfo::specialCaseVarIntros(ValueDecl *D, ASTContext *Context) {
  // Special-case for va_list, constrain to wild.
  bool IsGeneric = false;
  PVConstraint *PVC = nullptr;

  CVarOption CVOpt = getVariable(D, Context);
  if (CVOpt.hasValue()) {
    ConstraintVariable &CV = CVOpt.getValue();
    PVC = dyn_cast<PVConstraint>(&CV);
  }

  if (isa<ParmVarDecl>(D))
    IsGeneric = PVC && PVC->getIsGeneric();
  if (isVarArgType(D->getType().getAsString()) ||
      (hasVoidType(D) && !IsGeneric)) {
    // Set the reason for making this variable WILD.
    std::string Rsn = "Variable type void.";
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(D, *Context);
    if (!D->getType()->isVoidType())
      Rsn = "Variable type is va_list.";
    if (PVC != nullptr)
      PVC->constrainToWild(CS, Rsn, &PL);
  }
}

// For each pointer type in the declaration of D, add a variable to the
// constraint system for that pointer type.
void ProgramInfo::addVariable(clang::DeclaratorDecl *D,
                              clang::ASTContext *AstContext) {
  assert(!Persisted);

  PersistentSourceLoc PLoc = PersistentSourceLoc::mkPSL(D, *AstContext);
  assert(PLoc.valid());

  // We only add a PVConstraint if Variables[PLoc] does not exist.
  // Functions are exempt from this check because they need to be added to the
  // Extern/Static function map even if they are inside a macro expansion.
  if (Variables.find(PLoc) != Variables.end() && !isa<FunctionDecl>(D)) {
    // Two variables can have the same source locations when they are
    // declared inside the same macro expansion. The first instance of the
    // source location will have been constrained to WILD, so it's safe to bail
    // without doing anymore work.
    if (!Rewriter::isRewritable(D->getLocation())) {
      // If we're not in a macro, we should make the constraint variable WILD
      // anyways. This happens if the name of the variable is a macro defined
      // differently is different parts of the program.
      std::string Rsn = "Duplicate source location. Possibly part of a macro.";
      Variables[PLoc]->constrainToWild(CS, Rsn, &PLoc);
    }
    return;
  }

  ConstraintVariable *NewCV = nullptr;

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    // Function Decls have FVConstraints.
    FVConstraint *F = new FVConstraint(D, *this, *AstContext);
    F->setValidDecl();

    // Handling of PSL collision for functions is different since we need to
    // consider the static and extern function maps.
    if (Variables.find(PLoc) != Variables.end()) {
      // Try to find a previous definition based on function name
      if (!getFuncConstraint(FD, AstContext)) {
        // No function with the same name exists. It's concerning that
        // something already exists at this source location, but we add the
        // function to the function map anyways. The function map indexes by
        // function name, so there's no collision.
        insertNewFVConstraint(FD, F, AstContext);
        constrainWildIfMacro(F, FD->getLocation());
      } else {
        // A function with the same name exists in the same source location.
        // This happens when a function is defined in a header file which is
        // included in multiple translation units. getFuncConstraint returned
        // non-null, so we know that the definition has been processed already,
        // and there is no more work to do.
      }
      return;
    }

    // Store the FVConstraint in the global and Variables maps. It may be
    // merged with others if this is a redeclaration, and the merged version
    // is returned.
    F = insertNewFVConstraint(FD, F, AstContext);
    NewCV = F;

    auto RetTy = FD->getReturnType();
    unifyIfTypedef(RetTy.getTypePtr(), *AstContext, FD, F->getExternalReturn());
    unifyIfTypedef(RetTy.getTypePtr(), *AstContext, FD, F->getInternalReturn());

    // Add mappings from the parameters PLoc to the constraint variables for
    // the parameters.
    for (unsigned I = 0; I < FD->getNumParams(); I++) {
      ParmVarDecl *PVD = FD->getParamDecl(I);
      const Type *Ty = PVD->getType().getTypePtr();
      PVConstraint *PVInternal = F->getInternalParam(I);
      PVConstraint *PVExternal = F->getExternalParam(I);
      unifyIfTypedef(Ty, *AstContext, PVD, PVInternal);
      unifyIfTypedef(Ty, *AstContext, PVD, PVExternal);
      PVInternal->setValidDecl();
      PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(PVD, *AstContext);
      // Constraint variable is stored on the parent function, so we need to
      // constrain to WILD even if we don't end up storing this in the map.
      constrainWildIfMacro(PVExternal, PVD->getLocation());
      specialCaseVarIntros(PVD, AstContext);
      // It is possible to have a parameter decl in a macro when the function is
      // not.
      if (Variables.find(PSL) != Variables.end())
        continue;
      Variables[PSL] = PVInternal;
    }

  } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    assert(!isa<ParmVarDecl>(VD));
    const Type *Ty = VD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    if (Ty->isPointerType() || Ty->isArrayType()) {
      PVConstraint *P = new PVConstraint(D, *this, *AstContext);
      P->setValidDecl();
      NewCV = P;
      std::string VarName(VD->getName());
      unifyIfTypedef(Ty, *AstContext, VD, P);
      if (VD->hasGlobalStorage()) {
        // If we see a definition for this global variable, indicate so in
        // ExternGVars.
        if (VD->hasDefinition() || VD->hasDefinition(*AstContext)) {
          ExternGVars[VarName] = true;
        }
        // If we don't, check that we haven't seen one before before setting to
        // false.
        else if (!ExternGVars[VarName]) {
          ExternGVars[VarName] = false;
        }
        GlobalVariableSymbols[VarName].insert(P);
      }
      specialCaseVarIntros(D, AstContext);
    }

  } else if (FieldDecl *FlD = dyn_cast<FieldDecl>(D)) {
    const Type *Ty = FlD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    if (Ty->isPointerType() || Ty->isArrayType()) {
      PVConstraint *P = new PVConstraint(D, *this, *AstContext);
      unifyIfTypedef(Ty, *AstContext, FlD, P);
      NewCV = P;
      NewCV->setValidDecl();
      specialCaseVarIntros(D, AstContext);
    }
  } else
    llvm_unreachable("unknown decl type");

  assert("We shouldn't be adding a null CV to Variables map." && NewCV);
  if (!canWrite(PLoc.getFileName())) {
    NewCV->constrainToWild(CS, "Declaration in non-writable file", &PLoc);
  }
  constrainWildIfMacro(NewCV, D->getLocation());
  Variables[PLoc] = NewCV;
}

void ProgramInfo::unifyIfTypedef(const Type* Ty, ASTContext& Context, DeclaratorDecl* Decl, PVConstraint* P) {
  if (const auto* TDT = dyn_cast<TypedefType>(Ty)) {
    auto* TDecl = TDT->getDecl();
    auto PSL = PersistentSourceLoc::mkPSL(TDecl, Context);
    auto O = lookupTypedef(PSL);
    if (O.hasValue()) {
      auto *Bounds = &O.getValue();
      P->setTypedef(TDecl, TDecl->getNameAsString());
      constrainConsVarGeq(P, Bounds, CS, &PSL, Same_to_Same, true, this);
    }
  }
}


ProgramInfo::IDAndTranslationUnit
ProgramInfo::getExprKey(Expr *E, ASTContext *C) const {
  // TODO: Main file name can be shared by multiple translation units if on file
  //       is compiled multiple times with different defines
  std::string Name = C->getSourceManager().getFileEntryForID(
    C->getSourceManager().getMainFileID())->getName().str();
  return std::make_pair(E->getID(*C), Name);
}

bool ProgramInfo::hasPersistentConstraints(Expr *E, ASTContext *C) const {
  return ExprConstraintVars.find(getExprKey(E, C)) != ExprConstraintVars.end();
}

const CVarSet &ProgramInfo::getPersistentConstraintsSet(clang::Expr *E,
                                                        ASTContext *C) const {
  return getPersistentConstraints(E, C).first;
}

void ProgramInfo::storePersistentConstraints(clang::Expr *E,
                                             const CVarSet &Vars,
                                             ASTContext *C) {
  BKeySet EmptySet;
  EmptySet.clear();
  storePersistentConstraints(E, std::make_pair(Vars, EmptySet), C);
}

// Get the pair of set of constraint variables and set of bounds key
// for an expression that will persist between the constraint generation
// and rewriting pass. If the expression already has a set of persistent
// constraints, this set is returned. Otherwise, the set provided in the
// arguments is stored persistent and returned. This is required for
// correct cast insertion.
const CSetBkeyPair &ProgramInfo::getPersistentConstraints(Expr *E,
                                                     ASTContext *C) const {
  assert(hasPersistentConstraints(E, C) &&
         "Persistent constraints not present.");
  return ExprConstraintVars.at(getExprKey(E, C));
}

void ProgramInfo::storePersistentConstraints(Expr *E, const CSetBkeyPair &Vars,
                                             ASTContext *C) {
  assert(!hasPersistentConstraints(E, C) &&
         "Persistent constraints already present.");

   auto PSL = PersistentSourceLoc::mkPSL(E, *C);
   if (PSL.valid() && !canWrite(PSL.getFileName()))
     for (ConstraintVariable *CVar : Vars.first)
       CVar->constrainToWild(CS, "Expression in non-writable file", &PSL);

  IDAndTranslationUnit Key = getExprKey(E, C);
  ExprConstraintVars[Key] = Vars;
  ExprLocations[Key] = PSL;
}

// The Rewriter won't let us re-write things that are in macros. So, we
// should check to see if what we just added was defined within a macro.
// If it was, we should constrain it to top. This is sad. Hopefully,
// someday, the Rewriter will become less lame and let us re-write stuff
// in macros.
void ProgramInfo::constrainWildIfMacro(ConstraintVariable *CV,
                                       SourceLocation Location,
                                       PersistentSourceLoc *PSL) {
  std::string Rsn = "Pointer in Macro declaration.";
  if (!Rewriter::isRewritable(Location))
    CV->constrainToWild(CS, Rsn, PSL);
}

//std::string ProgramInfo::getUniqueDeclKey(Decl *D, ASTContext *C) {
//  auto Psl = PersistentSourceLoc::mkPSL(D, *C);
//  std::string FileName = Psl.getFileName() + ":" +
//                         std::to_string(Psl.getLineNo());
//  std::string Dname = D->getDeclKindName();
//  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
//    Dname = FD->getNameAsString();
//  }
//  std::string DeclKey = FileName + ":" + Dname;
//  return DeclKey;
//}
//
//std::string ProgramInfo::getUniqueFuncKey(FunctionDecl *D,
//                                          ASTContext *C) {
//  // Get unique key for a function: which is function name,
//  // file and line number.
//  if (FunctionDecl *FuncDef = getDefinition(D)) {
//    D = FuncDef;
//  }
//  return getUniqueDeclKey(D, C);
//}

FVConstraint *ProgramInfo::getFuncConstraint(FunctionDecl *D,
                                             ASTContext *C) const {
  std::string FuncName = D->getNameAsString();
  if (D->isGlobal()) {
    // Is this a global (externally visible) function?
    return getExtFuncDefnConstraint(FuncName);
  }
  // Static function.
  auto Psl = PersistentSourceLoc::mkPSL(D, *C);
  std::string FileName = Psl.getFileName();
  return getStaticFuncConstraint(FuncName, FileName);
}

FVConstraint *ProgramInfo::getFuncFVConstraint(FunctionDecl *FD,
                                               ASTContext *C) {
  std::string FuncName = FD->getNameAsString();
  FVConstraint *FunFVar = nullptr;
  if (FD->isGlobal()) {
    FunFVar = getExtFuncDefnConstraint(FuncName);
    // FIXME: We are being asked to access a function never declared; best
    // action?
    if (FunFVar == nullptr) {
      // make one
      FVConstraint *F = new FVConstraint(FD, *this, *C);
      assert(!F->hasBody());
      assert("FunFVar can only be null if FuncName is not in the map!" &&
             ExternalFunctionFVCons.find(FuncName) ==
                 ExternalFunctionFVCons.end());
      ExternalFunctionFVCons[FuncName] = F;
      FunFVar = ExternalFunctionFVCons[FuncName];
    }
  } else {
    auto Psl = PersistentSourceLoc::mkPSL(FD, *C);
    std::string FileName = Psl.getFileName();
    FunFVar = getStaticFuncConstraint(FuncName, FileName);
  }

  return FunFVar;
}

// Given a decl, return the variables for the constraints of the Decl.
// Returns null if a constraint variable could not be found for the decl.
CVarOption ProgramInfo::getVariable(clang::Decl *D, clang::ASTContext *C) {
  assert(!Persisted);

  if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
    DeclContext *DC = PD->getParentFunctionOrMethod();
    // This can fail for extern definitions
    if (!DC)
      return CVarOption();
    FunctionDecl *FD = dyn_cast<FunctionDecl>(DC);
    // Get the parameter index with in the function.
    unsigned int PIdx = getParameterIndex(PD, FD);
    // Get corresponding FVConstraint vars.
    FVConstraint *FunFVar = getFuncFVConstraint(FD, C);
    assert(FunFVar != nullptr && "Unable to find function constraints.");
    return CVarOption(*FunFVar->getInternalParam(PIdx));
  }
  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    FVConstraint *FunFVar = getFuncFVConstraint(FD, C);
    if (FunFVar == nullptr) {
      llvm::errs() << "No fun constraints for " << FD->getName() << "?!\n";
    }
    return CVarOption(*FunFVar);
  }
  /* neither function nor function parameter */
  auto I = Variables.find(PersistentSourceLoc::mkPSL(D, *C));
  if (I != Variables.end())
    return CVarOption(*I->second);
  return CVarOption();
}

FVConstraint *
ProgramInfo::getExtFuncDefnConstraint(std::string FuncName) const {
  if (ExternalFunctionFVCons.find(FuncName) != ExternalFunctionFVCons.end()) {
    return ExternalFunctionFVCons.at(FuncName);
  }
  return nullptr;
}

FVConstraint *ProgramInfo::getStaticFuncConstraint(std::string FuncName,
                                                   std::string FileName) const {
  if (StaticFunctionFVCons.find(FileName) != StaticFunctionFVCons.end() &&
      StaticFunctionFVCons.at(FileName).find(FuncName) !=
          StaticFunctionFVCons.at(FileName).end()) {
    return StaticFunctionFVCons.at(FileName).at(FuncName);
  }
  return nullptr;
}

// From the given constraint graph, this method computes the interim constraint
// state that contains constraint vars which are directly assigned WILD and
// other constraint vars that have been determined to be WILD because they
// depend on other constraint vars that are directly assigned WILD.
bool ProgramInfo::computeInterimConstraintState(
    const std::set<std::string> &FilePaths) {

  // Get all the valid vars of interest i.e., all the Vars that are present
  // in one of the files being compiled.
  CAtoms ValidVarsVec;
  std::set<Atom *> AllValidVars;
  CAtoms Tmp;
  for (const auto &I : Variables) {
    std::string FileName = I.first.getFileName();
    ConstraintVariable *C = I.second;
    if (C->isForValidDecl()) {
      Tmp.clear();
      getVarsFromConstraint(C, Tmp);
      AllValidVars.insert(Tmp.begin(), Tmp.end());
      if (canWrite(FileName))
        ValidVarsVec.insert(ValidVarsVec.begin(), Tmp.begin(), Tmp.end());
    }
  }

  // Make that into set, for efficiency.
  std::set<Atom *> ValidVarsS;
  ValidVarsS.insert(ValidVarsVec.begin(), ValidVarsVec.end());

  auto GetLocOrZero = [](const Atom *Val) {
    if (const auto *VA = dyn_cast<VarAtom>(Val))
      return VA->getLoc();
    return (ConstraintKey)0;
  };
  CVars ValidVarsKey;
  std::transform(ValidVarsS.begin(), ValidVarsS.end(),
                 std::inserter(ValidVarsKey, ValidVarsKey.end()), GetLocOrZero);
  CVars AllValidVarsKey;
  std::transform(AllValidVars.begin(), AllValidVars.end(),
                 std::inserter(AllValidVarsKey, AllValidVarsKey.end()),
                 GetLocOrZero);

  CState.clear();
  std::set<Atom *> DirectWildVarAtoms;
  CS.getChkCG().getSuccessors(CS.getWild(), DirectWildVarAtoms);

  // Maps each atom to the set of atoms which depend on it through an
  // implication constraint. These atoms would not be associated with the
  // correct root cause through a BFS because an explicit edge does not exist
  // between the cause and these atoms. Implication firing adds an edge from
  // WILD to the LHS conclusion ptr. The logical flow of WILDness, however, is
  // from the premise LHS to conclusion LHS.
  std::map<Atom *, std::set<Atom *>> ImpMap;
  for (auto *C : getConstraints().getConstraints())
    if (auto *Imp = dyn_cast<Implies>(C)) {
      auto *Pre = Imp->getPremise();
      auto *Con = Imp->getConclusion();
      ImpMap[Pre->getLHS()].insert(Con->getLHS());
    }

  CVars TmpCGrp;
  CVars OnlyIndirect;
  for (auto *A : DirectWildVarAtoms) {
    auto *VA = dyn_cast<VarAtom>(A);
    if (VA == nullptr)
      continue;

    TmpCGrp.clear();
    OnlyIndirect.clear();

    auto BFSVisitor = [&](Atom *SearchAtom) {
      auto *SearchVA = dyn_cast<VarAtom>(SearchAtom);
      if (SearchVA && AllValidVars.find(SearchVA) != AllValidVars.end()) {
        CState.RCMap[SearchVA->getLoc()].insert(VA->getLoc());
        TmpCGrp.insert(SearchVA->getLoc());
        if (DirectWildVarAtoms.find(SearchVA) == DirectWildVarAtoms.end()) {
          OnlyIndirect.insert(SearchVA->getLoc());
        }
      }
    };
    CS.getChkCG().visitBreadthFirst(VA, BFSVisitor);
    if (ImpMap.find(A) != ImpMap.end())
      for (Atom *ImpA : ImpMap[A])
        if (isa<VarAtom>(ImpA))
          CS.getChkCG().visitBreadthFirst(ImpA, BFSVisitor);

    CState.TotalNonDirectWildAtoms.insert(OnlyIndirect.begin(), OnlyIndirect.end());
    // Should we consider only pointers which with in the source files or
    // external pointers that affected pointers within the source files.
    CState.AllWildAtoms.insert(VA->getLoc());
    CVars &CGrp = CState.SrcWMap[VA->getLoc()];
    CGrp.insert(TmpCGrp.begin(), TmpCGrp.end());
  }
  findIntersection(CState.AllWildAtoms, ValidVarsKey, CState.InSrcWildAtoms);
  findIntersection(CState.TotalNonDirectWildAtoms, ValidVarsKey,
                   CState.InSrcNonDirectWildAtoms);

  for (const auto &I : Variables)
    insertIntoPtrSourceMap(&(I.first), I.second);
  for (const auto &I : ExprConstraintVars) {
    PersistentSourceLoc *PSL = &ExprLocations[I.first];
    for (auto *J : I.second.first)
      insertIntoPtrSourceMap(PSL, J);
  }

  auto &WildPtrsReason = CState.RootWildAtomsWithReason;
  for (auto *CurrC : CS.getConstraints()) {
    if (Geq *EC = dyn_cast<Geq>(CurrC)) {
      VarAtom *VLhs = dyn_cast<VarAtom>(EC->getLHS());
      if (EC->constraintIsChecked() && dyn_cast<WildAtom>(EC->getRHS())) {
        PersistentSourceLoc PSL = EC->getLocation();
        const PersistentSourceLoc *APSL = CState.AtomSourceMap[VLhs->getLoc()];
        if (!PSL.valid() && APSL && APSL->valid())
          PSL = *APSL;
        WildPointerInferenceInfo Info(EC->getReason(), PSL);
        WildPtrsReason.insert(std::make_pair(VLhs->getLoc(), Info));
      }
    }
  }

  computePtrLevelStats();
  return true;
}

void ProgramInfo::insertIntoPtrSourceMap(const PersistentSourceLoc *PSL,
                                         ConstraintVariable *CV) {
  std::string FilePath = PSL->getFileName();
  if (canWrite(FilePath))
    CState.ValidSourceFiles.insert(FilePath);
  else
    return;

  if (auto *PV = dyn_cast<PVConstraint>(CV)) {
    for (auto *A : PV->getCvars())
      if (auto *VA = dyn_cast<VarAtom>(A))
        CState.AtomSourceMap[VA->getLoc()] = PSL;
    // If the PVConstraint is a function pointer, create mappings for parameter
    // and return variables.
    if (auto *FV = PV->getFV()) {
      insertIntoPtrSourceMap(PSL, FV->getExternalReturn());
      for (unsigned int I = 0; I < FV->numParams(); I++)
        insertIntoPtrSourceMap(PSL, FV->getExternalParam(I));
    }
  } else if (auto *FV = dyn_cast<FVConstraint>(CV)) {
    insertIntoPtrSourceMap(PSL, FV->getExternalReturn());
  }
}

void ProgramInfo::insertCVAtoms(
    ConstraintVariable *CV,
    std::map<ConstraintKey, ConstraintVariable *> &AtomMap) {
  if (auto *PVC = dyn_cast<PVConstraint>(CV)) {
    for (Atom *A : PVC->getCvars())
      if (auto *VA = dyn_cast<VarAtom>(A)) {
        // It is possible that VA->getLoc() already exists in the map if there
        // is a function which is declared before it is defined.
        assert(AtomMap.find(VA->getLoc()) == AtomMap.end() ||
               PVC->isPartOfFunctionPrototype());
        AtomMap[VA->getLoc()] = PVC;
      }
    if (FVConstraint *FVC = PVC->getFV())
      insertCVAtoms(FVC, AtomMap);
  } else if (auto *FVC = dyn_cast<FVConstraint>(CV)) {
    insertCVAtoms(FVC->getExternalReturn(), AtomMap);
    for (unsigned I = 0; I < FVC->numParams(); I++)
      insertCVAtoms(FVC->getExternalParam(I), AtomMap);
  } else {
    llvm_unreachable("Unknown kind of constraint variable.");
  }
}

void ProgramInfo::computePtrLevelStats() {
  // Construct a map from Atoms to their containing constraint variable
  std::map<ConstraintKey, ConstraintVariable *> AtomPtrMap;
  for (const auto &I : Variables)
    insertCVAtoms(I.second, AtomPtrMap);

  // Populate maps with per-pointer root cause information
  for (auto Entry : CState.RCMap) {
    assert("RCMap entry is not mapped to a pointer!" &&
           AtomPtrMap.find(Entry.first) != AtomPtrMap.end());
    ConstraintVariable *CV = AtomPtrMap[Entry.first];
    for (auto RC : Entry.second)
      CState.PtrRCMap[CV].insert(RC);
  }
  for (auto Entry : CState.SrcWMap) {
    for (auto Key : Entry.second) {
      assert(AtomPtrMap.find(Key) != AtomPtrMap.end());
      CState.PtrSrcWMap[Entry.first].insert(AtomPtrMap[Key]);
    }
  }
}

void ProgramInfo::setTypeParamBinding(CallExpr *CE, unsigned int TypeVarIdx,
                                      ConstraintVariable *CV, ASTContext *C) {

  auto PSL = PersistentSourceLoc::mkPSL(CE, *C);
  auto CallMap = TypeParamBindings[PSL];
  if (CallMap.find(TypeVarIdx) == CallMap.end()) {
    TypeParamBindings[PSL][TypeVarIdx] = CV;
  } else {
    // If this CE/idx is at the same location, it's in a macro,
    // so mark it as inconsistent.
    TypeParamBindings[PSL][TypeVarIdx] = nullptr;
  }
}

bool ProgramInfo::hasTypeParamBindings(CallExpr *CE, ASTContext *C) const {
  auto PSL = PersistentSourceLoc::mkPSL(CE, *C);
  return TypeParamBindings.find(PSL) != TypeParamBindings.end();
}

const ProgramInfo::CallTypeParamBindingsT &
ProgramInfo::getTypeParamBindings(CallExpr *CE, ASTContext *C) const {
  auto PSL = PersistentSourceLoc::mkPSL(CE, *C);
  assert("Type parameter bindings could not be found." &&
         TypeParamBindings.find(PSL) != TypeParamBindings.end());
  return TypeParamBindings.at(PSL);
}

CVarOption ProgramInfo::lookupTypedef(PersistentSourceLoc PSL) {
  return TypedefVars[PSL];
}

bool ProgramInfo::seenTypedef(PersistentSourceLoc PSL) {
  return TypedefVars.count(PSL) != 0;
}

void ProgramInfo::addTypedef(PersistentSourceLoc PSL, bool CanRewriteDef,
                             TypedefDecl* TD, ASTContext &C) {
  auto Name = TD->getNameAsString();
  ConstraintVariable* V = nullptr;
  const auto T = TD->getUnderlyingType();
  if (isa<clang::FunctionProtoType>(T) || isa<clang::FunctionNoProtoType>(T)) 
    V = new FunctionVariableConstraint(T.getTypePtr(), 
        nullptr, Name, *this, C);
   else  
    V = new PointerVariableConstraint(T, nullptr, Name, *this, C);
  auto *const Rsn =
      !CanRewriteDef ?
           "Unable to rewrite a typedef with multiple names"
            : "Declaration in non-writable file";
  if (!(CanRewriteDef && canWrite(PSL.getFileName()))) {
    V->constrainToWild(this->getConstraints(), Rsn, &PSL);
  }
  constrainWildIfMacro(V, TD->getLocation(), &PSL);
  this->TypedefVars[PSL] = {*V};
}
