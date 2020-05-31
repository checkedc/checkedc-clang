//=--ConstraintResolver.cpp---------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of methods in ConstraintResolver.h that help in fetching
// constraints for a given expression.
//===----------------------------------------------------------------------===//

#include "clang/CConv/ConstraintResolver.h"
#include "clang/CConv/CCGlobalOptions.h"

using namespace llvm;
using namespace clang;

std::set<ConstraintVariable *> ConstraintResolver::TempConstraintVars;

ConstraintResolver::~ConstraintResolver() {
  // No need to free the memory. The memory should be relased explicitly
  // by calling releaseTempConsVars
  ExprTmpConstraints.clear();
}

void ConstraintResolver::constraintAllCVarsToWild(
    std::set<ConstraintVariable *> &CSet, std::string rsn, Expr *AtExpr) {
  PersistentSourceLoc Psl;
  PersistentSourceLoc *PslP = nullptr;
  if (AtExpr != nullptr) {
    Psl = PersistentSourceLoc::mkPSL(AtExpr, *Context);
    PslP = &Psl;
  }

  for (const auto &A : CSet) {
    if (PVConstraint *PVC = dyn_cast<PVConstraint>(A))
      PVC->constrainToWild(Info.getConstraints(), rsn, PslP);
  }
}

std::set<ConstraintVariable *>
ConstraintResolver::getExprConstraintVars(Expr *E,
                                          QualType LhsType, bool Ifc,
                                          bool NonEmptyCons) {
  std::set<ConstraintVariable *> TmpCons;
  std::set<ConstraintVariable *> RvalCons;

  bool IsAssigned;
  std::set<ConstraintVariable *> ExprCons =
      getExprConstraintVars(TmpCons, E, RvalCons, LhsType, IsAssigned, Ifc);

  if (ExprCons.empty() && NonEmptyCons && !IsAssigned) {
    ExprCons = RvalCons;
  }
  return ExprCons;
}

std::set<ConstraintVariable *>
    ConstraintResolver::handleDeref(std::set<ConstraintVariable *> T) {
  std::set<ConstraintVariable *> tmp;
  for (const auto &CV : T) {
    PVConstraint *PVC = dyn_cast<PVConstraint>(CV);
    assert (PVC != nullptr); // Shouldn't be dereferencing FPs
    // Subtract one from this constraint. If that generates an empty
    // constraint, then, don't add it
    CAtoms C = PVC->getCvars();
    if (C.size() > 0) {
      C.erase(C.begin());
      if (C.size() > 0) {
        bool a = PVC->getArrPresent();
        bool c = PVC->getItypePresent();
        std::string d = PVC->getItype();
        FVConstraint *b = PVC->getFV();
        PVConstraint *TmpPV = new PVConstraint(C, PVC->getTy(), PVC->getName(),
                                               b, a, c, d);
        TempConstraintVars.insert(TmpPV);
        tmp.insert(TmpPV);
      }
    }
  }
  return tmp;
}

// Update a PVConstraint with one additional level of indirection
PVConstraint *ConstraintResolver::addAtom(PVConstraint *PVC,
                                          Atom *NewA, Constraints &CS) {
  CAtoms C = PVC->getCvars();
  if (!C.empty()) {
    Atom *A = *C.begin();
    // If PVC is already a pointer, add implication forcing outermost
    //   one to be wild if this added one is
    if (VarAtom *VA = dyn_cast<VarAtom>(A)) {
      NewA = CS.getFreshVar("&"+(PVC->getName()), VarAtom::V_Other);
      auto *Prem = CS.createGeq(VA, CS.getWild());
      auto *Conc = CS.createGeq(NewA, CS.getWild());
      CS.addConstraint(CS.createImplies(Prem, Conc));
    } else if (ConstAtom *C = dyn_cast<WildAtom>(A)) {
      NewA = CS.getWild();
    } // else stick with what's given
  }
  C.insert(C.begin(), NewA);
  bool a = PVC->getArrPresent();
  FVConstraint *b = PVC->getFV();
  bool c = PVC->getItypePresent();
  std::string d = PVC->getItype();
  PVConstraint *TmpPV = new PVConstraint(C, PVC->getTy(), PVC->getName(),
                                         b, a, c, d);
  TempConstraintVars.insert(TmpPV);
  return TmpPV;
}

// Processes E from malloc(E) to discern the pointer type this will be
static Atom *analyzeAllocExpr(Expr *E, Constraints &CS, QualType &ArgTy) {
  Atom *ret = CS.getPtr();
  BinaryOperator *B = dyn_cast<BinaryOperator>(E);
  std::set<Expr *> Exprs;

  // Looking for X*Y -- could be an array
  if (B && B->isMultiplicativeOp()) {
    ret = CS.getArr();
    Exprs.insert(B->getLHS());
    Exprs.insert(B->getRHS());
  }
  else
    Exprs.insert(E);

  // Look for sizeof(X); return Arr or Ptr if found
  for (Expr *Ex: Exprs) {
    UnaryExprOrTypeTraitExpr *arg = dyn_cast<UnaryExprOrTypeTraitExpr>(Ex);
    if (arg && arg->getKind() == UETT_SizeOf) {
      ArgTy = arg->getTypeOfArgument();
      return ret;
    }
  }
  return nullptr;
}
ConstraintVariable *
ConstraintResolver::getTemporaryConstraintVariable(clang::Expr *E,
                                                   ConstraintVariable *CV) {
  auto ExpKey = std::make_pair(E, CV);
  if (ExprTmpConstraints.find(ExpKey) == ExprTmpConstraints.end()) {
    // Make a copy and store the copy of the underlying constraint
    // into TempConstraintVars to handle memory management.
    auto *CVarPtr = CV->getCopy(Info.getConstraints());
    TempConstraintVars.insert(CVarPtr);
    ExprTmpConstraints[ExpKey] = CVarPtr;
  }
  return ExprTmpConstraints[ExpKey];
}

// We traverse the AST in a
// bottom-up manner, and, for a given expression, decide which singular,
// if any, constraint variable is involved in that expression. However,
// in the current version of clang (3.8.1), bottom-up traversal is not
// supported. So instead, we do a manual top-down traversal, considering
// the different cases and their meaning on the value of the constraint
// variable involved. This is probably incomplete, but, we're going to
// go with it for now.
//
// V is (currentVariable, baseVariable, limitVariable)
// E is an expression to recursively traverse.
//
// Returns true if E resolves to a constraint variable q_i and the
// currentVariable field of V is that constraint variable. Returns false if
// a constraint variable cannot be found.
// ifc mirrors the inFunctionContext boolean parameter to getVariable.
std::set<ConstraintVariable *> ConstraintResolver::getExprConstraintVars(
    std::set<ConstraintVariable *> &LHSConstraints, Expr *E,
    std::set<ConstraintVariable *> &RvalCons, QualType LhsType,
    bool &IsAssigned, bool Ifc) {
  if (E != nullptr) {
    auto &CS = Info.getConstraints();
    E = E->IgnoreParenImpCasts();
    E = getNormalizedExpr(E);
    bool TmpAssign = false;
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
      return Info.getVariable(DRE->getDecl(), Context);
    } else if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
      return Info.getVariable(ME->getMemberDecl(), Context);
    } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
      bool LHSAssigned = false, RHSAssigned = false;
      std::set<ConstraintVariable *> T1 = getExprConstraintVars(
          LHSConstraints, BO->getLHS(), RvalCons, LhsType, LHSAssigned, Ifc);
      std::set<ConstraintVariable *> T2 = getExprConstraintVars(
          LHSConstraints, BO->getRHS(), RvalCons, LhsType, RHSAssigned, Ifc);
      if (T1.empty() && !LHSAssigned && T2.empty() && !RHSAssigned) {
        T1 = RvalCons;
        IsAssigned = false;
      } else {
        T1.insert(T2.begin(), T2.end());
        IsAssigned = T1.empty();
      }
      return T1;
    } else if (ArraySubscriptExpr *AE = dyn_cast<ArraySubscriptExpr>(E)) {
      // In an array subscript, we want to do something sort of similar to
      // taking the address or doing a dereference.
      std::set<ConstraintVariable *> T = getExprConstraintVars(
          LHSConstraints, AE->getBase(), RvalCons, LhsType, TmpAssign, Ifc);
      std::set<ConstraintVariable *> tmp = handleDeref(T);
      T.swap(tmp);
      return T;
    } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
      std::set<ConstraintVariable *> T = getExprConstraintVars(
          LHSConstraints, UO->getSubExpr(), RvalCons, LhsType, TmpAssign, Ifc);

      std::set<ConstraintVariable *> tmp;
      if (UO->getOpcode() == UO_Deref)
        tmp = handleDeref(T);
      else if (UO->getOpcode() == UO_AddrOf) {
        if (T.empty()) { // doing &x where x is a non-pointer
          tmp.insert(PVConstraint::getPtrPVConstraint(Info.getConstraints()));
        } else {
          assert(T.size() == 1 && "AddrOf only for lvals; shouldn't have multiple PVconstraints");
          auto &CV = *T.begin();
          if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
            tmp.insert(addAtom(PVC, CS.getPtr(), CS));
          } // no-op for FPs
        }
      }
      T.swap(tmp);
      return T;
    } else if (ImplicitCastExpr *IE = dyn_cast<ImplicitCastExpr>(E)) {
      return getExprConstraintVars(LHSConstraints, IE->getSubExpr(), RvalCons,
                               LhsType, IsAssigned, Ifc);
    } else if (ExplicitCastExpr *ECE = dyn_cast<ExplicitCastExpr>(E)) {
      Expr *TmpE = removeAuxillaryCasts(ECE->getSubExpr());
      std::set<ConstraintVariable *> TmpCons = getExprConstraintVars(
          LHSConstraints, TmpE, RvalCons, LhsType, IsAssigned, Ifc);
      // Is cast compatible with LHS type?
      if (!Info.isExplicitCastSafe(LhsType, ECE->getType())) {
        constraintAllCVarsToWild(TmpCons, "Casted to a different type.", E);
        constraintAllCVarsToWild(LHSConstraints,
                                 "Casted From a different type.", E);
      }
      return TmpCons;
    } else if (ParenExpr *PE = dyn_cast<ParenExpr>(E)) {
      return getExprConstraintVars(LHSConstraints, PE->getSubExpr(), RvalCons,
                               LhsType, IsAssigned, Ifc);
    } else if (CHKCBindTemporaryExpr *CBE =
                   dyn_cast<CHKCBindTemporaryExpr>(E)) {
      return getExprConstraintVars(LHSConstraints, CBE->getSubExpr(), RvalCons,
                               LhsType, IsAssigned, Ifc);
    } else if (CallExpr *CE = dyn_cast<CallExpr>(E)) {
      // Call expression should always get out-of context constraint variable.
      Ifc = false;
      std::set<ConstraintVariable *> TR;
      // Here, we need to look up the target of the call and return the
      // constraints for the return value of that function.
      QualType ExprType = E->getType();
      Decl *D = CE->getCalleeDecl();
      if (D == nullptr) {
        // There are a few reasons that we couldn't get a decl. For example,
        // the call could be done through an array subscript.
        Expr *CalledExpr = CE->getCallee();
        std::set<ConstraintVariable *> tmp = getExprConstraintVars(
            LHSConstraints, CalledExpr, RvalCons, LhsType, IsAssigned, Ifc);

        for (ConstraintVariable *C : tmp) {
          if (FVConstraint *FV = dyn_cast<FVConstraint>(C)) {
            TR.insert(FV->getReturnVars().begin(), FV->getReturnVars().end());
          } else if (PVConstraint *PV = dyn_cast<PVConstraint>(C)) {
            if (FVConstraint *FV = PV->getFV()) {
              TR.insert(FV->getReturnVars().begin(), FV->getReturnVars().end());
            }
          }
        }
      } else if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(D)) {
        // D could be a FunctionDecl, or a VarDecl, or a FieldDecl.
        // Really it could be any DeclaratorDecl.
        if (isFunctionAllocator(FD->getName())) {
          bool didInsert = false;
          // FIXME: Should be treating malloc, realloc, calloc differently
          if (CE->getNumArgs() > 0) {
            QualType ArgTy;
            Atom *A = analyzeAllocExpr(CE->getArg(0), CS, ArgTy);
            if (A) {
              std::string N = FD->getName(); N = "&"+N;
              PVConstraint *PVC =
                  new PVConstraint(ArgTy, nullptr, N, CS,*Context);
              TempConstraintVars.insert(PVC);
              PVConstraint *PVCaddr = addAtom(PVC, A,CS);
              TR.insert(PVCaddr);
              didInsert = true;
              ExprType = Context->getPointerType(ArgTy);
            }
          }
          if (!didInsert)
            TR.insert(PVConstraint::getWildPVConstraint(Info.getConstraints()));
        } else { // normal function call
          std::set<ConstraintVariable *> CS = Info.getVariable(FD, Context);
          FVConstraint *FVC = nullptr;
          for (const auto &J : CS) {
            if (FVConstraint *tmp = dyn_cast<FVConstraint>(J))
              // The constraint we retrieved is a function constraint already.
              // This happens if what is being called is a reference to a
              // function declaration, but it isn't all that can happen.
              FVC = tmp;
            else if (PVConstraint *tmp = dyn_cast<PVConstraint>(J))
              if (FVConstraint *tmp2 = tmp->getFV())
                // Or, we could have a PVConstraint to a function pointer.
                // In that case, the function pointer value will work just
                // as well.
                FVC = tmp2;
          }

          if (FVC) {
            TR.insert(FVC->getReturnVars().begin(), FVC->getReturnVars().end());
          } else {
            // Our options are slim. For some reason, we have failed to find a
            // FVConstraint for the Decl that we are calling. This can't be good
            // so we should constrain everything in the caller to top. We can
            // fake this by returning a nullary-ish FVConstraint and that will
            // make the logic above us freak out and over-constrain everything.
            auto *TmpFV = new FVConstraint();
            TempConstraintVars.insert(TmpFV);
            TR.insert(TmpFV);
          }
        }
      } else {
        // If it ISN'T, though... what to do? How could this happen?
        llvm_unreachable("TODO");
      }

      // This is R-Value, we need to make a copy of the resulting
      // ConstraintVariables.
      std::set<ConstraintVariable *> TmpCVs;
      for (ConstraintVariable *CV : TR) {
        ConstraintVariable *NewCV = getTemporaryConstraintVariable(CE, CV);
        constrainConsVarGeq(NewCV, CV, CS, nullptr, Safe_to_Wild, false, &Info);
        TmpCVs.insert(NewCV);
      }

      if (!Info.isExplicitCastSafe(LhsType, ExprType)) {
        constraintAllCVarsToWild(TmpCVs, "Assigning to a different type.", E);
        constraintAllCVarsToWild(LHSConstraints,
                                 "Assigned from a different type.", E);
      }

      // If LHS constraints are not empty? Assign to LHS.
      if (!LHSConstraints.empty()) {
        auto PL = PersistentSourceLoc::mkPSL(CE, *Context);
        constrainConsVarGeq(LHSConstraints, TmpCVs, CS, &PL, Safe_to_Wild,
                            false, &Info);
        // We assigned the constraints to the LHS.
        // We do not need to propagate the constraints.
        IsAssigned = true;
        TmpCVs.clear();
      }
      return TmpCVs;
    } else if (ConditionalOperator *CO = dyn_cast<ConditionalOperator>(E)) {
      std::set<ConstraintVariable *> T;
      std::set<ConstraintVariable *> R;
      bool TAssign = false, RAssign = false;
      // The condition is not what's returned by the expression, so do not
      // include its var T = getVariableHelper(CO->getCond(), V, C, Ifc);
      // R.insert(T.begin(), T.end());
      T = getExprConstraintVars(LHSConstraints, CO->getLHS(), RvalCons, LhsType,
                            TAssign, Ifc);
      if (T.empty() && !TAssign) {
        R.insert(RvalCons.begin(), RvalCons.end());
      } else {
        R.insert(T.begin(), T.end());
      }
      T = getExprConstraintVars(LHSConstraints, CO->getRHS(), RvalCons, LhsType,
                            RAssign, Ifc);
      if (T.empty() && !RAssign) {
        R.insert(RvalCons.begin(), RvalCons.end());
      } else {
        R.insert(T.begin(), T.end());
      }
      IsAssigned = TAssign && RAssign;
      return R;
    } else if (clang::StringLiteral *exr = dyn_cast<clang::StringLiteral>(E)) {
      // If this is a string literal. i.e., "foo".
      // We create a new constraint variable and constraint it to an Nt_array.
      std::set<ConstraintVariable *> T;
      // Create a new constraint var number and make it NTArr.
      CAtoms V;
      V.push_back(CS.getNTArr());
      ConstraintVariable *newC = new PointerVariableConstraint(
          V, "const char*", exr->getBytes(), nullptr, false, false, "");
      TempConstraintVars.insert(newC);
      T.insert(newC);
      return T;

    } else if (isNULLExpression(E, *Context)) {
      // Do Nothing.
      return std::set<ConstraintVariable *>();
    } else if (E->isIntegerConstantExpr(*Context) &&
               !E->isNullPointerConstant(*Context,
                                         Expr::NPC_ValueDependentIsNotNull)) {
      // Return WILD ins R constraint
      auto TmpCvs = getWildPVConstraint();
      RvalCons.insert(TmpCvs.begin(), TmpCvs.end());
      // Return empty
      return std::set<ConstraintVariable *>();
    } else {
      return std::set<ConstraintVariable *>();
    }
  }
  return std::set<ConstraintVariable *>();
}

void ConstraintResolver::constrainLocalAssign(Stmt *TSt, Expr *LHS, Expr *RHS,
                                             ConsAction CAction) {
  PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(TSt, *Context);
  // Get the in-context local constraints.
  std::set<ConstraintVariable *> L =
      getExprConstraintVars(LHS, LHS->getType(), true);
  std::set<ConstraintVariable *> TmpValueCons;
  TmpValueCons.clear();
  bool IsAssigned = false;
  std::set<ConstraintVariable *> R =
      getExprConstraintVars(L, RHS, TmpValueCons, LHS->getType(),
                            IsAssigned, true);
  // If this is not assigned? Get RVale Cons
  if (!IsAssigned) {
    if (R.empty()) {
      R = TmpValueCons;
    }
    constrainConsVarGeq(L, R, Info.getConstraints(), &PL,
                        CAction, false, &Info);
  }
}

void ConstraintResolver::constrainLocalAssign(Stmt *TSt, DeclaratorDecl *D,
                                              Expr *RHS,
                                             ConsAction CAction) {
  bool IsAssigned = false;
  std::set<ConstraintVariable *> TmpValueCons;
  PersistentSourceLoc PL, *PLPtr = nullptr;
  if (TSt != nullptr) {
   PL = PersistentSourceLoc::mkPSL(TSt, *Context);
   PLPtr = &PL;
  }
  // Get the in-context local constraints.
  std::set<ConstraintVariable *> V = Info.getVariable(D, Context);
  auto RHSCons =
      getExprConstraintVars(V, RHS, TmpValueCons, D->getType(),
                            IsAssigned, true);

  if (!V.empty() && RHSCons.empty() && !IsAssigned) {
    RHSCons.insert(TmpValueCons.begin(), TmpValueCons.end());
  }
  constrainConsVarGeq(V, RHSCons, Info.getConstraints(),
                      PLPtr, CAction, false, &Info);
}

std::set<ConstraintVariable *> ConstraintResolver::getWildPVConstraint() {
  std::set<ConstraintVariable *> Ret;
  Ret.insert(PVConstraint::getWildPVConstraint(Info.getConstraints()));
  return Ret;
}