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
#include "llvm/Support/JSON.h"
#include <sstream>

using namespace clang;

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
    // The `FuncName` and `FileName` field names are backwards: this is actually
    // the file name, hence the need to defend against special characters.
    O << "{\"FuncName\":" << llvm::json::Value(DefM.first)
      << ", \"Constraints\":[";
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

    O << "{\"line\":";
    O << llvm::json::Value(L.toString());
    O << ",\"Variables\":[";
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
static void getVarsFromConstraint(ConstraintVariable *V, CAtoms &R,
                                  std::set<ConstraintVariable *> &Visited) {
  if (Visited.find(V) == Visited.end()) {
    Visited.insert(V);
    if (auto *PVC = dyn_cast_or_null<PVConstraint>(V)) {
      R.insert(R.begin(), PVC->getCvars().begin(), PVC->getCvars().end());
      if (FVConstraint *FVC = PVC->getFV())
        getVarsFromConstraint(FVC, R, Visited);
    } else if (auto *FVC = dyn_cast_or_null<FVConstraint>(V)) {
      getVarsFromConstraint(FVC->getInternalReturn(), R, Visited);
      for (unsigned I = 0; I < FVC->numParams(); I++)
        getVarsFromConstraint(FVC->getInternalParam(I), R, Visited);
    }
  }
}

// Print aggregate stats
void ProgramInfo::printAggregateStats(const std::set<std::string> &F,
                                      llvm::raw_ostream &O) {
  std::vector<Atom *> AllAtoms;
  CVarSet Visited;
  CAtoms FoundVars;

  unsigned int TotP, TotNt, TotA, TotWi;
  TotP = TotNt = TotA = TotWi = 0;

  CVarSet ArrPtrs, NtArrPtrs;
  ConstraintVariable *Tmp = nullptr;

  for (auto &I : Variables) {
    ConstraintVariable *C = I.second;
    std::string FileName = I.first.getFileName();
    if (F.count(FileName) ||
        FileName.find(_3COpts.BaseDir) != std::string::npos) {
      if (C->isForValidDecl()) {
        FoundVars.clear();
        getVarsFromConstraint(C, FoundVars, Visited);
        std::copy(FoundVars.begin(), FoundVars.end(),
                  std::back_inserter(AllAtoms));
        Tmp = C;
        if (FVConstraint *FV = dyn_cast<FVConstraint>(C)) {
          Tmp = FV->getInternalReturn();
        }
        // If this is a var atom?
        if (Tmp->hasNtArr(CS.getVariables(), 0)) {
          NtArrPtrs.insert(Tmp);
        } else if (Tmp->hasArr(CS.getVariables(), 0)) {
          ArrPtrs.insert(Tmp);
        }
      }
    }
  }

  for (const auto &N : AllAtoms) {
    ConstAtom *CA = CS.getAssignment(N);
    switch (CA->getKind()) {
    case Atom::A_Arr:
      TotA += 1;
      break;
    case Atom::A_NTArr:
      TotNt += 1;
      break;
    case Atom::A_Ptr:
      TotP += 1;
      break;
    case Atom::A_Wild:
      TotWi += 1;
      break;
    case Atom::A_Var:
    case Atom::A_Const:
      llvm_unreachable("bad constant in environment map");
    }
  }

  O << "{\"AggregateStats\":[";
  O << "{\""
    << "TotalStats"
    << "\":{";
  O << "\"constraints\":" << AllAtoms.size() << ",";
  O << "\"ptr\":" << TotP << ",";
  O << "\"ntarr\":" << TotNt << ",";
  O << "\"arr\":" << TotA << ",";
  O << "\"wild\":" << TotWi;
  O << "}},";
  O << "{\"ArrBoundsStats\":";
  ArrBInfo.printStats(O, ArrPtrs, true);
  O << "},";
  O << "{\"NtArrBoundsStats\":";
  ArrBInfo.printStats(O, NtArrPtrs, true);
  O << "},";
  O << "{\"PerformanceStats\":";
  PerfS.printPerformanceStats(O, true);
  O << "}";
  O << "]}";
}

// Print out statistics of constraint variables on a per-file basis.
void ProgramInfo::printStats(const std::set<std::string> &F, raw_ostream &O,
                             bool OnlySummary, bool JsonFormat) {
  if (!OnlySummary && !JsonFormat) {
    O << "Sound handling of var args functions:" << _3COpts.HandleVARARGS
      << "\n";
  }
  std::map<std::string, std::tuple<int, int, int, int, int>> FilesToVars;
  CVarSet InSrcCVars, Visited;
  unsigned int TotC, TotP, TotNt, TotA, TotWi;
  TotC = TotP = TotNt = TotA = TotWi = 0;

  // First, build the map and perform the aggregation.
  for (auto &I : Variables) {
    std::string FileName = I.first.getFileName();
    if (F.count(FileName) ||
        FileName.find(_3COpts.BaseDir) != std::string::npos) {
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
        getVarsFromConstraint(C, FoundVars, Visited);

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
        O << "{" << llvm::json::Value(I.first) << ":{";
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

  if (_3COpts.AllTypes) {
    if (JsonFormat) {
      O << "\"BoundsStats\":";
    }
    ArrBInfo.printStats(O, InSrcCVars, JsonFormat);
    if (JsonFormat)
      O << ",";
  }

  if (JsonFormat) {
    O << "\"PerformanceStats\":";
  }

  PerfS.printPerformanceStats(O, JsonFormat);

  if (JsonFormat) {
    O << "}}";
  }
}

bool ProgramInfo::link() {
  // For every global symbol in all the global symbols that we have found
  // go through and apply rules for whether they are functions or variables.
  if (_3COpts.Verbose)
    llvm::errs() << "Linking!\n";

  auto Rsn = ReasonLoc("Linking global variables",
                       PersistentSourceLoc());

  // Equate the constraints for all global variables.
  // This is needed for variables that are defined as extern.
  for (const auto &V : GlobalVariableSymbols) {
    const std::set<PVConstraint *> &C = V.second;

    if (C.size() > 1) {
      std::set<PVConstraint *>::iterator I = C.begin();
      std::set<PVConstraint *>::iterator J = C.begin();
      ++J;
      if (_3COpts.Verbose)
        llvm::errs() << "Global variables:" << V.first << "\n";
      while (J != C.end()) {
        constrainConsVarGeq(*I, *J, CS, Rsn, Same_to_Same, true, this);
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
      auto WildReason = ReasonLoc(
          "External global variable " + VarName + " has no definition",
          Rsn.Location);
      const std::set<PVConstraint *> &C = GlobalVariableSymbols[VarName];
      for (const auto &Var : C) {
        // TODO: Is there an easy way to get a PSL to attach to the constraint?
        Var->constrainToWild(CS, WildReason);
      }
    }
  }

  // For every global function that is an unresolved external, constrain
  // its parameter types to be wild. Unless it has a bounds-safe annotation.
  for (const auto &U : ExternalFunctionFVCons)
    linkFunction(U.second);

  // Repeat for static functions.
  //
  // Static functions that don't have a body will always cause a linking
  // error during compilation. They may still be useful as code is developed,
  // so we treat them as if they are external, and constrain parameters
  // to wild as appropriate.
  for (const auto &U : StaticFunctionFVCons)
    for (const auto &V : U.second)
      linkFunction(V.second);

  return true;
}

void ProgramInfo::linkFunction(FunctionVariableConstraint *FV) {
  // If there was a checked type on a variable in the input program, it
  // should stay that way. Otherwise, we shouldn't be adding a checked type
  // to an undefined function. DEFAULT_REASON is a sentinel for
  // ConstraintVariable::equateWithItype; see the comment there.
  std::string Rsn = (FV->hasBody() ? DEFAULT_REASON : "Unchecked pointer in parameter or "
                                          "return of undefined function " +
                                          FV->getName());

  // Handle the cases where itype parameters should not be treated as their
  // unchecked type.
  // TODO: Ditto re getting a PSL (in the case in which Rsn is non-empty and
  // it is actually used).
  auto Reason = ReasonLoc(Rsn, PersistentSourceLoc());
  FV->equateWithItype(*this, Reason);

  // Used to apply constraints to parameters and returns for function without a
  // body. In the default configuration, the function is fully constrained so
  // that parameters and returns are considered unchecked. When 3C is run with
  // --infer-types-for-undefs, only internal variables are constrained, allowing
  // external variables to solve to checked types meaning the parameter will be
  // rewritten to an itype.
  auto LinkComponent = [this, Reason](const FVComponentVariable *FVC) {
    FVC->getInternal()->constrainToWild(CS, Reason);
    if (!_3COpts.InferTypesForUndefs &&
        !FVC->getExternal()->srcHasItype() && !FVC->getExternal()->isGeneric())
      FVC->getExternal()->constrainToWild(CS, Reason);
  };

  if (!FV->hasBody()) {
    LinkComponent(FV->getCombineReturn());
    for (unsigned I = 0; I < FV->numParams(); I++)
      LinkComponent(FV->getCombineParam(I));
  }
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
    if (StaticFunctionFVCons.find(FileName) == StaticFunctionFVCons.end()) {
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

  auto *OldC = (*Map)[FuncName];
  std::string ReasonFailed = "";
  int OldCount = OldC->numParams();
  int NewCount = NewC->numParams();

  // merge short parameter lists into long ones
  // Choose number of params, but favor definitions if available
  if ((OldCount < NewCount) ||
      (OldCount == NewCount && !OldC->hasBody() && NewC->hasBody())) {
    NewC->mergeDeclaration(OldC, *this, ReasonFailed);
    (*Map)[FuncName] = NewC;
  } else {
    OldC->mergeDeclaration(NewC, *this, ReasonFailed);
  }

  // If successful, we're done and can skip error reporting
  if (ReasonFailed == "")
    return (*Map)[FuncName];

  // Error reporting
  reportCustomDiagnostic(C->getDiagnostics(),
                         DiagnosticsEngine::Fatal,
                         "merging failed for %q0 due to %1",
                         FD->getLocation())
      << FD << ReasonFailed;
  // A failed merge will provide poor data, but the diagnostic error report
  // will cause the program to terminate after the variable adder step.
  return (*Map)[FuncName];
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
      Variables[PLoc]->constrainToWild(CS, ReasonLoc(Rsn, PLoc));
    }
    return;
  }

  ConstraintVariable *NewCV = nullptr;

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    // Function Decls have FVConstraints.
    std::string FuncName = FD->getNameAsString();
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
        constrainWildIfMacro(F, FD->getLocation(), ReasonLoc(MACRO_REASON, PLoc));
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
    unifyIfTypedef(RetTy, *AstContext, F->getExternalReturn(), Wild_to_Safe);
    unifyIfTypedef(RetTy, *AstContext, F->getInternalReturn(), Safe_to_Wild);
    auto PSL = PersistentSourceLoc::mkPSL(FD,*AstContext);
    ensureNtCorrect(RetTy, PSL, F->getExternalReturn());
    ensureNtCorrect(RetTy, PSL, F->getInternalReturn());

    // Add mappings from the parameters PLoc to the constraint variables for
    // the parameters.
    for (unsigned I = 0; I < FD->getNumParams(); I++) {
      ParmVarDecl *PVD = FD->getParamDecl(I);
      auto ParamPSL = PersistentSourceLoc::mkPSL(PVD,*AstContext);
      QualType ParamTy = PVD->getType();
      PVConstraint *PVInternal = F->getInternalParam(I);
      PVConstraint *PVExternal = F->getExternalParam(I);
      unifyIfTypedef(ParamTy, *AstContext, PVExternal, Wild_to_Safe);
      unifyIfTypedef(ParamTy, *AstContext, PVInternal, Safe_to_Wild);
      ensureNtCorrect(ParamTy, ParamPSL, PVInternal);
      ensureNtCorrect(ParamTy, ParamPSL, PVExternal);
      PVInternal->setValidDecl();
      PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(PVD, *AstContext);
      // Constraint variable is stored on the parent function, so we need to
      // constrain to WILD even if we don't end up storing this in the map.
      constrainWildIfMacro(PVExternal, PVD->getLocation(),
                           ReasonLoc(MACRO_REASON, PSL));
      // If this is "main", constrain its argv parameter to a nested arr
      if (_3COpts.AllTypes && FuncName == "main" && FD->isGlobal() && I == 1) {
        PVInternal->constrainOuterTo(CS, CS.getArr(),
                                     ReasonLoc(SPECIAL_REASON("main"), PSL));
        PVInternal->constrainIdxTo(CS, CS.getNTArr(), 1,
                                   ReasonLoc(SPECIAL_REASON("main"), PSL));
      }
      // It is possible to have a param decl in a macro when the function is
      // not.
      if (Variables.find(PSL) != Variables.end())
        continue;
      Variables[PSL] = PVInternal;
    }

  } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    assert(!isa<ParmVarDecl>(VD));
    QualType QT = VD->getTypeSourceInfo()->getTypeLoc().getType();
    if (QT->isPointerType() || QT->isArrayType()) {
      PVConstraint *P = new PVConstraint(D, *this, *AstContext);
      P->setValidDecl();
      NewCV = P;
      std::string VarName(VD->getName());
      unifyIfTypedef(QT, *AstContext, P);
      auto PSL = PersistentSourceLoc::mkPSL(VD, *AstContext);
      ensureNtCorrect(VD->getType(), PSL, P);
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
    }

  } else if (FieldDecl *FlD = dyn_cast<FieldDecl>(D)) {
    QualType QT = FlD->getTypeSourceInfo()->getTypeLoc().getType();
    if (QT->isPointerType() || QT->isArrayType()) {
      PVConstraint *P = new PVConstraint(D, *this, *AstContext);
      unifyIfTypedef(QT, *AstContext, P);
      NewCV = P;
      NewCV->setValidDecl();
      if (FlD->getParent()->isUnion()) {
        auto Rsn = ReasonLoc(UNION_FIELD_REASON, PLoc);
        NewCV->equateWithItype(*this, Rsn);
        NewCV->constrainToWild(CS, Rsn);
      }
    }
  } else
    llvm_unreachable("unknown decl type");

  assert("We shouldn't be adding a null CV to Variables map." && NewCV);
  if (!canWrite(PLoc.getFileName())) {
    auto Rsn = ReasonLoc(UNWRITABLE_REASON, PLoc);
    NewCV->equateWithItype(*this, Rsn);
    NewCV->constrainToWild(CS, Rsn);
  }
  constrainWildIfMacro(NewCV, D->getLocation(), ReasonLoc(MACRO_REASON, PLoc));
  Variables[PLoc] = NewCV;
}

void ProgramInfo::ensureNtCorrect(const QualType &QT,
                                  const PersistentSourceLoc &PSL,
                                  PointerVariableConstraint *PV) {
  if (_3COpts.AllTypes && !canBeNtArray(QT)) {
    PV->constrainOuterTo(CS, CS.getArr(),
                         ReasonLoc(ARRAY_REASON, PSL), true, true);
  }
}

void ProgramInfo::unifyIfTypedef(const QualType &QT, ASTContext &Context,
                                 PVConstraint *P, ConsAction CA) {
  if (const auto *TDT = dyn_cast<TypedefType>(QT.getTypePtr())) {
    auto *TDecl = TDT->getDecl();
    auto PSL = PersistentSourceLoc::mkPSL(TDecl, Context);
    auto O = lookupTypedef(PSL);
    auto Rsn = ReasonLoc("typedef", PSL);
    if (O.hasValue()) {
      auto *Bounds = &O.getValue();
      P->setTypedef(Bounds, TDecl->getNameAsString());
      constrainConsVarGeq(P, Bounds, CS, Rsn, CA, false, this);
    }
  }
}

ProgramInfo::IDAndTranslationUnit ProgramInfo::getExprKey(Expr *E,
                                                          ASTContext *C) const {
  return std::make_pair(getStmtIdWorkaround(E, *C),
                        TranslationUnitIdxMap.at(C));
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
      CVar->constrainToWild(CS, ReasonLoc(UNWRITABLE_REASON, PSL));

  IDAndTranslationUnit Key = getExprKey(E, C);
  ExprConstraintVars[Key] = Vars;
  ExprLocations[Key] = PSL;
}

void ProgramInfo::removePersistentConstraints(Expr *E, ASTContext *C) {
  assert(hasPersistentConstraints(E, C) &&
         "Persistent constraints not present.");

  IDAndTranslationUnit Key = getExprKey(E, C);

  // Save VarAtom locations so they can be used to assign source locations to
  // root causes.
  for (auto *CV : ExprConstraintVars[Key].first)
    if (auto *PVC  = dyn_cast<PointerVariableConstraint>(CV))
      for (Atom *A : PVC->getCvars())
        if (auto *VA = dyn_cast<VarAtom>(A))
          DeletedAtomLocations[VA->getLoc()] = ExprLocations[Key];

  ExprConstraintVars.erase(Key);
  ExprLocations.erase(Key);
}

// The Rewriter won't let us re-write things that are in macros. So, we
// should check to see if what we just added was defined within a macro.
// If it was, we should constrain it to top. This is sad. Hopefully,
// someday, the Rewriter will become less lame and let us re-write stuff
// in macros.
void ProgramInfo::constrainWildIfMacro(ConstraintVariable *CV,
                                       SourceLocation Location,
                                       const ReasonLoc &Rsn) {
  if (!Rewriter::isRewritable(Location))
    CV->constrainToWild(CS, Rsn);
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
  CVarSet Visited;
  CAtoms Tmp;
  for (const auto &I : Variables) {
    std::string FileName = I.first.getFileName();
    ConstraintVariable *C = I.second;
    if (C->isForValidDecl()) {
      Tmp.clear();
      getVarsFromConstraint(C, Tmp, Visited);
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

        if (ValidVarsKey.find(SearchVA->getLoc()) != ValidVarsKey.end())
          TmpCGrp.insert(SearchVA->getLoc());
        if (DirectWildVarAtoms.find(SearchVA) == DirectWildVarAtoms.end()) {
          OnlyIndirect.insert(SearchVA->getLoc());
        }
      }
    };
    CS.getChkCG().visitBreadthFirst(VA, BFSVisitor);

    CState.TotalNonDirectWildAtoms.insert(OnlyIndirect.begin(),
                                          OnlyIndirect.end());
    // Should we consider only pointers which with in the source files or
    // external pointers that affected pointers within the source files.
    CState.AllWildAtoms.insert(VA->getLoc());
    CVars &CGrp = CState.SrcWMap[VA->getLoc()];
    CGrp.insert(TmpCGrp.begin(), TmpCGrp.end());
  }
  findIntersection(CState.AllWildAtoms, ValidVarsKey, CState.InSrcWildAtoms);
  findIntersection(CState.TotalNonDirectWildAtoms, ValidVarsKey,
                   CState.InSrcNonDirectWildAtoms);

  // The ConstraintVariable for a variable normally appears in Variables for the
  // definition, but it may also be reused directly in ExprConstraintVars for a
  // reference to that variable. We want to give priority to the PSL of the
  // definition, not the reference. We currently achieve this by processing
  // Variables before ExprConstraintVars and making insertIntoPtrSourceMap not
  // overwrite a PSL already recorded for a given atom.
  for (const auto &I : Variables)
    insertIntoPtrSourceMap(I.first, I.second);
  for (const auto &I : ExprConstraintVars) {
    PersistentSourceLoc PSL = ExprLocations[I.first];
    for (auto *J : I.second.first)
      insertIntoPtrSourceMap(PSL, J);
  }
  for (auto E : DeletedAtomLocations)
    CState.AtomSourceMap.insert(std::make_pair(E.first, E.second));

  auto &WildPtrsReason = CState.RootWildAtomsWithReason;
  for (Constraint *CurrC : CS.getConstraints()) {
    if (Geq *EC = dyn_cast<Geq>(CurrC)) {
      VarAtom *VLhs = dyn_cast<VarAtom>(EC->getLHS());
      if (EC->constraintIsChecked() && dyn_cast<WildAtom>(EC->getRHS())) {
        PersistentSourceLoc PSL = EC->getLocation();
        PersistentSourceLoc APSL = CState.AtomSourceMap[VLhs->getLoc()];
        if (!PSL.valid() && APSL.valid())
          PSL = APSL;
        auto Rsn = ReasonLoc(EC->getReasonText(), PSL);
        RootCauseDiagnostic Info(Rsn);
        for (const auto &Reason : CurrC->additionalReasons()) {
          PersistentSourceLoc P = Reason.Location;
          if (!P.valid() && APSL.valid())
            P = APSL;
          Info.addReason(ReasonLoc(Reason.Reason, P));
        }
        WildPtrsReason.insert(std::make_pair(VLhs->getLoc(), Info));
      }
    }
  }

  computePtrLevelStats();
  return true;
}

void ProgramInfo::insertIntoPtrSourceMap(PersistentSourceLoc PSL,
                                         ConstraintVariable *CV) {
  std::string FilePath = PSL.getFileName();
  if (canWrite(FilePath))
    CState.ValidSourceFiles.insert(FilePath);

  if (auto *PV = dyn_cast<PVConstraint>(CV)) {
    for (auto *A : PV->getCvars())
      if (auto *VA = dyn_cast<VarAtom>(A))
        // Don't overwrite a PSL already recorded for a given atom: see the
        // comment in computeInterimConstraintState.
        CState.AtomSourceMap.insert(std::make_pair(VA->getLoc(), PSL));
    // If the PVConstraint is a function pointer, create mappings for parameter
    // and return variables.
    if (auto *FV = PV->getFV()) {
      insertIntoPtrSourceMap(PSL, FV->getInternalReturn());
      for (unsigned int I = 0; I < FV->numParams(); I++)
        insertIntoPtrSourceMap(PSL, FV->getInternalParam(I));
    }
  } else if (auto *FV = dyn_cast<FVConstraint>(CV)) {
    insertIntoPtrSourceMap(PSL, FV->getInternalReturn());
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
    insertCVAtoms(FVC->getInternalReturn(), AtomMap);
    for (unsigned I = 0; I < FVC->numParams(); I++)
      insertCVAtoms(FVC->getInternalParam(I), AtomMap);
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
                                      ConstraintVariable *CV,
                                      ConstraintVariable *Ident,
                                      ASTContext *C) {

  auto Key = getExprKey(CE, C);
  auto CallMap = TypeParamBindings[Key];
  if (CallMap.find(TypeVarIdx) == CallMap.end()) {
    TypeParamBindings[Key][TypeVarIdx] = TypeParamConstraint(CV,Ident);
  } else {
    // If this CE/idx is at the same location, it's in a macro,
    // so mark it as inconsistent.
    TypeParamBindings[Key][TypeVarIdx] = TypeParamConstraint(nullptr,nullptr);
  }
}

bool ProgramInfo::hasTypeParamBindings(CallExpr *CE, ASTContext *C) const {
  auto Key = getExprKey(CE, C);
  return TypeParamBindings.find(Key) != TypeParamBindings.end();
}

const ProgramInfo::CallTypeParamBindingsT &
ProgramInfo::getTypeParamBindings(CallExpr *CE, ASTContext *C) const {
  auto Key = getExprKey(CE, C);
  assert("Type parameter bindings could not be found." &&
         TypeParamBindings.find(Key) != TypeParamBindings.end());
  return TypeParamBindings.at(Key);
}

CVarOption ProgramInfo::lookupTypedef(PersistentSourceLoc PSL) {
  return TypedefVars[PSL];
}

bool ProgramInfo::seenTypedef(PersistentSourceLoc PSL) {
  return TypedefVars.count(PSL) != 0;
}

void ProgramInfo::addTypedef(PersistentSourceLoc PSL, TypedefDecl *TD,
                             ASTContext &C) {
  ConstraintVariable *V = nullptr;
  if (isa<clang::FunctionType>(TD->getUnderlyingType()))
    V = new FunctionVariableConstraint(TD, *this, C);
  else
    V = new PointerVariableConstraint(TD, *this, C);

  if (!canWrite(PSL.getFileName()))
    V->constrainToWild(this->getConstraints(),
                       ReasonLoc(UNWRITABLE_REASON, PSL));

  constrainWildIfMacro(V, TD->getLocation(), ReasonLoc(MACRO_REASON, PSL));
  this->TypedefVars[PSL] = {*V};
}

void ProgramInfo::registerTranslationUnits(
    const std::vector<std::unique_ptr<clang::ASTUnit>> &ASTs) {
  assert(TranslationUnitIdxMap.empty());
  unsigned int Idx = 0;
  for (const auto &AST : ASTs) {
    TranslationUnitIdxMap[&(AST->getASTContext())] = Idx;
    Idx++;
  }
}
