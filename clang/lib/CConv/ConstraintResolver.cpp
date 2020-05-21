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

std::set<ConstraintVariable *> ConstraintResolver::GlobalRValueCons;

// Special-case handling for decl introductions. For the moment this covers:
//  * void-typed variables
//  * va_list-typed variables
void ConstraintResolver::specialCaseVarIntros(ValueDecl *D, bool FuncCtx) {
  // Constrain everything that is void to wild.
  Constraints &CS = Info.getConstraints();

  // Special-case for va_list, constrain to wild.
  if (isVarArgType(D->getType().getAsString()) || hasVoidType(D)) {
    // set the reason for making this variable WILD.
    std::string Rsn = "Variable type void.";
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(D, *Context);
    if (!D->getType()->isVoidType())
      Rsn = "Variable type is va_list.";
    for (const auto &I : Info.getVariable(D, Context, FuncCtx)) {
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(I)) {
        PVC->constrainToWild(CS, Rsn, &PL, false);
      }
    }
  }
}

bool ConstraintResolver::handleFuncCall(CallExpr *CA, QualType LhsType) {
  bool RulesFired = false;
  // Is this a call to malloc? Can we coerce the callee
  // to a NamedDecl?
  FunctionDecl *CalleeDecl = dyn_cast<FunctionDecl>(CA->getCalleeDecl());
  // FIXME: Right now we don't look at what malloc is doing
  // but I don't think this works in the new regime.
  if (CalleeDecl && isFunctionAllocator(CalleeDecl->getName())) {
    // This is an allocator, should we treat it as safe?
    if (!ConsiderAllocUnsafe) {
      RulesFired = true;
    } else {
      // It's a call to allocator.
      // What about the parameter to the call?
      if (CA->getNumArgs() > 0) {
        UnaryExprOrTypeTraitExpr *arg =
            dyn_cast<UnaryExprOrTypeTraitExpr>(CA->getArg(0));
        if (arg && arg->isArgumentType()) {
          // Check that the argument is a sizeof.
          if (arg->getKind() == UETT_SizeOf) {
            QualType ArgTy = arg->getArgumentType();
            // argTy should be made a pointer, then compared for
            // equality to lhsType and rhsTy.
            QualType ArgPTy = Context->getPointerType(ArgTy);

            if (Info.isExplicitCastSafe(ArgPTy, LhsType)) {
              RulesFired = true;
              // At present, I don't think we need to add an
              // implication based constraint since this rule
              // only fires if there is a cast from a call to malloc.
              // Since malloc is an external, there's no point in
              // adding constraints to it.
            }
          }
        }
      }
    }
  }
  return RulesFired;
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
      PVC->constrainToWild(Info.getConstraints(), rsn, PslP, false);
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

// This is a bit of a hack. What we need to do is traverse the AST in a
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
      return Info.getVariable(DRE->getDecl(), Context, Ifc);
    } else if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
      return Info.getVariable(ME->getMemberDecl(), Context, Ifc);
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
      std::set<ConstraintVariable *> tmp;
      for (const auto &CV : T) {
        if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
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
              tmp.insert(new PVConstraint(C, PVC->getTy(), PVC->getName(), b, a,
                                          c, d));
            }
          }
        }
      }

      T.swap(tmp);
      return T;
    } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
      std::set<ConstraintVariable *> T = getExprConstraintVars(
          LHSConstraints, UO->getSubExpr(), RvalCons, LhsType, TmpAssign, Ifc);

      std::set<ConstraintVariable *> tmp;
      if (UO->getOpcode() == UO_Deref || UO->getOpcode() == UO_AddrOf) {
        for (const auto &CV : T) {
          if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
            CAtoms C = PVC->getCvars();
            if (UO->getOpcode() == UO_Deref) {
              // Subtract one from this constraint. If that generates an empty
              // constraint, then, don't add it
              if (C.size() > 0) {
                C.erase(C.begin());
                if (C.size() > 0) {
                  bool a = PVC->getArrPresent();
                  FVConstraint *b = PVC->getFV();
                  bool c = PVC->getItypePresent();
                  std::string d = PVC->getItype();
                  tmp.insert(new PVConstraint(C, PVC->getTy(), PVC->getName(),
                                              b, a, c, d));
                }
              }
            } else { // AddrOf
              Atom *NewA = CS.getPtr();
              // This is for & operand.
              // Here, we need to make sure that &(unchecked ptr) should be
              // unchecked too.
              // Lets add an implication.
              if (!C.empty()) {
                Atom *A = *C.begin();
                if (VarAtom *VA = dyn_cast<VarAtom>(A)) {
                  NewA = CS.getFreshVar("&q", VarAtom::V_Other);
                  auto *Prem = CS.createGeq(VA, CS.getWild());
                  auto *Conc = CS.createGeq(NewA, CS.getWild());
                  CS.addConstraint(CS.createImplies(Prem, Conc));
                } else if (ConstAtom *C = dyn_cast<WildAtom>(A)) {
                  NewA = CS.getWild();
                } // else stick with PTR
              }
              C.insert(C.begin(), NewA);
              bool a = PVC->getArrPresent();
              FVConstraint *b = PVC->getFV();
              bool c = PVC->getItypePresent();
              std::string d = PVC->getItype();
              tmp.insert(new PVConstraint(C, PVC->getTy(), PVC->getName(), b, a,
                                          c, d));
            }
          } else if (!(UO->getOpcode() == UO_AddrOf)) { // no-op for FPs
            llvm_unreachable("Shouldn't dereference a function pointer!");
          }
        }
        T.swap(tmp);
      }

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
      if (!handleFuncCall(CE, LhsType)) {
        // Call expression should always get out-of context
        // constraint variable.
        Ifc = false;
        std::set<ConstraintVariable *> TR;
        // Here, we need to look up the target of the call and return the
        // constraints for the return value of that function.
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
                TR.insert(FV->getReturnVars().begin(),
                          FV->getReturnVars().end());
              }
            }
          }
        } else if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(D)) {
          // D could be a FunctionDecl, or a VarDecl, or a FieldDecl.
          // Really it could be any DeclaratorDecl.
          std::set<ConstraintVariable *> CS =
              Info.getVariable(FD, Context, Ifc);
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
            TR.insert(new FVConstraint());
          }
        } else {
          // If it ISN'T, though... what to do? How could this happen?
          llvm_unreachable("TODO");
        }

        // This is R-Value, we need to make a copy of the resulting
        // ConstraintVariables.
        std::set<ConstraintVariable *> TmpCVs;
        for (ConstraintVariable *CV : TR) {
          ConstraintVariable *NewCV = CV->getCopy(CS);
          // Store the temporary constraint vars into a global set
          // for future memory management.
          GlobalRValueCons.insert(NewCV);
          constrainConsVarGeq(NewCV, CV, CS, nullptr, Safe_to_Wild,
                              false, &Info);
          TmpCVs.insert(NewCV);
        }

        if (!Info.isExplicitCastSafe(LhsType, E->getType())) {
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
      } else {
        return std::set<ConstraintVariable *>();
      }
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
  std::set<ConstraintVariable *> V = Info.getVariable(D, Context, true);
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