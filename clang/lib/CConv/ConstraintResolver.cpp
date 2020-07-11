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
  // No need to free the memory. The memory should be released explicitly
  // by calling releaseTempConsVars
  ExprTmpConstraints.clear();
}

// Force all ConstraintVariables in this set to be WILD
void ConstraintResolver::constraintAllCVarsToWild(
    std::set<ConstraintVariable *> &CSet, std::string rsn, Expr *AtExpr) {
  PersistentSourceLoc Psl;
  PersistentSourceLoc *PslP = nullptr;
  if (AtExpr != nullptr) {
    Psl = PersistentSourceLoc::mkPSL(AtExpr, *Context);
    PslP = &Psl;
  }
  auto &CS = Info.getConstraints();

  for (const auto &A : CSet) {
    if (PVConstraint *PVC = dyn_cast<PVConstraint>(A))
      PVC->constrainToWild(CS, rsn, PslP);
    else {
      FVConstraint *FVC = dyn_cast<FVConstraint>(A);
      assert(FVC != nullptr);
      FVC->constrainToWild(CS, rsn, PslP);
    }
  }
}

// Return a set of PVConstraints equivalent to the set given,
// but dereferenced one level down
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
        bool c = PVC->hasItype();
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

// For each constraint variable either invoke addAtom to add an additional level
// of indirection (when the constraint is PVConstraint), or return the constraint
// unchanged (when the constraint is a function constraint).
std::set<ConstraintVariable *>
    ConstraintResolver::addAtomAll(std::set<ConstraintVariable *> CVS,
                                   ConstAtom *PtrTyp, Constraints &CS) {
  std::set<ConstraintVariable *> Result;
  for (auto *CV : CVS) {
    if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
      PVConstraint *temp = addAtom(PVC, PtrTyp, CS);
      Result.insert(temp);
    } else {
      Result.insert(CV);
    }
  }
  return Result;
}

// Add to a PVConstraint one additional level of indirection
// The pointer type of the new atom is constrained >= PtrTyp.
PVConstraint *ConstraintResolver::addAtom(PVConstraint *PVC, ConstAtom *PtrTyp, Constraints &CS) {
  Atom *NewA = CS.getFreshVar("&"+(PVC->getName()), VarAtom::V_Other);
  CAtoms C = PVC->getCvars();
  if (!C.empty()) {
    Atom *A = *C.begin();
    // If PVC is already a pointer, add implication forcing outermost
    //   one to be wild if this added one is
    if (VarAtom *VA = dyn_cast<VarAtom>(A)) {
      auto *Prem = CS.createGeq(NewA, CS.getWild());
      auto *Conc = CS.createGeq(VA, CS.getWild());
      CS.addConstraint(CS.createImplies(Prem, Conc));
    }
  }

  C.insert(C.begin(), NewA);
  bool a = PVC->getArrPresent();
  FVConstraint *b = PVC->getFV();
  bool c = PVC->hasItype();
  std::string d = PVC->getItype();
  PVConstraint *TmpPV = new PVConstraint(C, PVC->getTy(), PVC->getName(),
                                         b, a, c, d);
  TmpPV->constrainOuterTo(CS, PtrTyp, true);
  TempConstraintVars.insert(TmpPV);
  return TmpPV;
}

// Processes E from malloc(E) to discern the pointer type this will be
static ConstAtom *analyzeAllocExpr(CallExpr *CE, Constraints &CS, QualType &ArgTy,
    std::string FuncName, ASTContext *Context) {
  if (FuncName.compare("calloc") == 0) {
    ArgTy = CE->getArg(1)->getType();
    // Check if first argument to calloc is 1
    Expr *E = CE->getArg(0);
    Expr::EvalResult res;
    E->EvaluateAsInt(res, *Context,
                     clang::Expr::SE_NoSideEffects, false);
    if (res.Val.isInt() && res.Val.getInt().getExtValue() == 1)
      return CS.getPtr();
    else
      return CS.getNTArr();
  }

  ConstAtom *ret = CS.getPtr();
  Expr *E;
  if (FuncName.compare("malloc") == 0)
    E = CE->getArg(0);
  else {
    assert(FuncName.compare("realloc") == 0);
    E = CE->getArg(1);
  }
  E = E->IgnoreParenImpCasts();
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

std::set<ConstraintVariable *>
    ConstraintResolver::getInvalidCastPVCons(Expr *E) {
  QualType SrcType, DstType;
  DstType = E->getType();
  SrcType = E->getType();
  if (ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E)) {
    SrcType = ICE->getSubExpr()->getType();
  }
  if (ExplicitCastExpr *ECE = dyn_cast<ExplicitCastExpr>(E)) {
    SrcType = ECE->getSubExpr()->getType();
  }
  std::set<ConstraintVariable *> Ret;
  auto &CS = Info.getConstraints();
  if (hasPersistentConstraints(E)) {
    return getPersistentConstraints(E,Ret);
  }
  CAtoms NewVA;
  Atom *NewA = CS.getFreshVar("Invalid cast to:" + E->getType().getAsString(),
                              VarAtom::V_Other);
  NewVA.push_back(NewA);

  PVConstraint *P = new PVConstraint(NewVA, "unsigned", "wildvar",
                                     nullptr, false, false, "");
  PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(E, *Context);
  P->constrainToWild(CS, "Casted from " + SrcType.getAsString() +  " to " +
                             DstType.getAsString() , &PL);

  Ret = {P};

  return Ret;

}

// Returns a set of ConstraintVariables which represent the result of
// evaluating the expression E. Will explore E recursively, but will
// ignore parts of it that do not contribute to the final result
std::set<ConstraintVariable *>
    ConstraintResolver::getExprConstraintVars(Expr *E) {
  if (E != nullptr) {
    auto &CS = Info.getConstraints();
    QualType TypE = E->getType();
    E = E->IgnoreParens();

    // Non-pointer (int, char, etc.) types have a special base PVConstraint
    if (TypE->isRecordType() || TypE->isArithmeticType()) {
      if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
        // If we have a DeclRef, the PVC can get a meaningful name
        return getBaseVarPVConstraint(DRE);
      } else {
        return PVConstraintFromType(TypE);
      }

    // NULL
    } else if (isNULLExpression(E, *Context)) {
      return std::set<ConstraintVariable *>();

    // Implicit cast, e.g., T* from T[] or int (*)(int) from int (int),
    //   but also weird int->int * conversions (and back)
    } else if (ImplicitCastExpr *IE = dyn_cast<ImplicitCastExpr>(E)) {
      QualType SubTypE = IE->getSubExpr()->getType();
      auto CVs = getExprConstraintVars(IE->getSubExpr());
      // if TypE is a pointer type, and the cast is unsafe, return WildPtr
      if (TypE->isPointerType()
          && !(SubTypE->isFunctionType()
               || SubTypE->isArrayType()
               || SubTypE->isVoidPointerType())
          && !isCastSafe(TypE, SubTypE)) {
        constraintAllCVarsToWild(CVs, "Casted to a different type.", IE);
        return getInvalidCastPVCons(E);
      }
      // else, return sub-expression's result
      return CVs;

    // (T)e
    } else if (ExplicitCastExpr *ECE = dyn_cast<ExplicitCastExpr>(E)) {
      assert(ECE->getType() == TypE);
      // Is cast internally safe? Return WILD if not
      Expr *TmpE = ECE->getSubExpr();
      if (TypE->isPointerType() && !isCastSafe(TypE, TmpE->getType()))
        return getInvalidCastPVCons(E);
        // NB: Expression ECE itself handled in ConstraintBuilder::FunctionVisitor
      else
        return getExprConstraintVars(TmpE);

    // variable (x)
    } else if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
      return Info.getVariable(DRE->getDecl(), Context);

    // x.f
    } else if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
      return Info.getVariable(ME->getMemberDecl(), Context);

    // x = y, x+y, x+=y, etc.
    } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
      switch (BO->getOpcode()) {
      /* Assignment, comma operators; only care about LHS */
      case BO_Assign:
      case BO_AddAssign:
      case BO_SubAssign:
        return getExprConstraintVars(BO->getLHS());
      case BO_Comma:
        return getExprConstraintVars(BO->getRHS());
      /* Possible pointer arithmetic: Could be LHS or RHS */
      case BO_Add:
      case BO_Sub:
        if (BO->getLHS()->getType()->isPointerType())
          return getExprConstraintVars(BO->getLHS());
        else if (BO->getRHS()->getType()->isPointerType())
          return getExprConstraintVars(BO->getRHS());
        else
          return PVConstraintFromType(TypE);
      /* Pointer-to-member ops unsupported */
      case BO_PtrMemD:
      case BO_PtrMemI:
        assert(false && "Bogus pointer-to-member operator");
        break;
      /* bit-shift/arithmetic/assign/comp operators return ints; do nothing */
      case BO_ShlAssign:
      case BO_ShrAssign:
      case BO_AndAssign:
      case BO_XorAssign:
      case BO_OrAssign:
      case BO_MulAssign:
      case BO_DivAssign:
      case BO_RemAssign:
      case BO_And:
      case BO_Or:
      case BO_Mul:
      case BO_Div:
      case BO_Rem:
      case BO_Xor:
      case BO_Cmp:
      case BO_EQ:
      case BO_NE:
      case BO_GE:
      case BO_GT:
      case BO_LE:
      case BO_LT:
      case BO_LAnd:
      case BO_LOr:
      case BO_Shl:
      case BO_Shr:
        return PVConstraintFromType(TypE);
      }

    // x[e]
    } else if (ArraySubscriptExpr *AE = dyn_cast<ArraySubscriptExpr>(E)) {
      std::set<ConstraintVariable *> T = getExprConstraintVars(AE->getBase());
      std::set<ConstraintVariable *> tmp = handleDeref(T);
      T.swap(tmp);
      return T;

    // ++e, &e, *e, etc.
    } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
      Expr *UOExpr = UO->getSubExpr();
      switch (UO->getOpcode()) {
      // &e
      // C99 6.5.3.2: "The operand of the unary & operator shall be either a
      // function designator, the result of a [] or unary * operator, or an
      // lvalue that designates an object that is not a bit-field and is not
      // declared with the register storage-class specifier."
      case UO_AddrOf: {
        std::set<ConstraintVariable *> AddrVs;
        if (hasPersistentConstraints(UO)) {
          return getPersistentConstraints(UO, AddrVs);
        }

        UOExpr = UOExpr->IgnoreParenImpCasts();
        // Taking the address of a dereference is a NoOp, so the constraint
        // vars for the subexpression can be passed through.
        // FIXME: We've dumped implicit casts on UOEXpr; restore?
        if (UnaryOperator *SubUO = dyn_cast<UnaryOperator>(UOExpr)) {
          if (SubUO->getOpcode() == UO_Deref)
            return getExprConstraintVars(SubUO->getSubExpr());
          // else, fall through
        } else if (ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(UOExpr)) {
          return getExprConstraintVars(ASE->getBase());
        }
        // add a VarAtom to UOExpr's PVConstraint, for &
        std::set<ConstraintVariable *> T = getExprConstraintVars(UOExpr);
        assert("Empty constraint vars in AddrOf!" && !T.empty());
        AddrVs = addAtomAll(T, CS.getPtr(), CS);

        return getPersistentConstraints(UO, AddrVs);
      }

      // *e
      case UO_Deref: {
        // We are dereferencing, so don't assign to LHS
        std::set<ConstraintVariable *> T = getExprConstraintVars(UOExpr);
        return handleDeref(T);
      }
      /* Operations on lval; if pointer, just process that */
      // e++, e--, ++e, --e
      case UO_PostInc:
      case UO_PostDec:
      case UO_PreInc:
      case UO_PreDec:
        return getExprConstraintVars(UOExpr);
      /* Integer operators */
      // +e, -e, ~e
      case UO_Plus:
      case UO_Minus:
      case UO_LNot:
      case UO_Not:
        return PVConstraintFromType(TypE);
      case UO_Coawait:
      case UO_Real:
      case UO_Imag:
      case UO_Extension:
        assert(false && "Unsupported unary operator");
        break;
      }

    // f(e1,e2, ...)
    } else if (CallExpr *CE = dyn_cast<CallExpr>(E)) {
      // Call expression should always get out-of context constraint variable.
      std::set<ConstraintVariable *> ReturnCVs;
      if (hasPersistentConstraints(CE)) {
        return getPersistentConstraints(CE, ReturnCVs);
      }

      // Here, we need to look up the target of the call and return the
      // constraints for the return value of that function.
      QualType ExprType = E->getType();
      Decl *D = CE->getCalleeDecl();
      std::set<ConstraintVariable *> ReallocFlow;
      if (D == nullptr) {
        // There are a few reasons that we couldn't get a decl. For example,
        // the call could be done through an array subscript.
        Expr *CalledExpr = CE->getCallee();
        std::set<ConstraintVariable *> tmp = getExprConstraintVars(CalledExpr);

        for (ConstraintVariable *C : tmp) {
          if (FVConstraint *FV = dyn_cast<FVConstraint>(C)) {
            ReturnCVs.insert(FV->getReturnVars().begin(), FV->getReturnVars().end());
          } else if (PVConstraint *PV = dyn_cast<PVConstraint>(C)) {
            if (FVConstraint *FV = PV->getFV()) {
              ReturnCVs.insert(FV->getReturnVars().begin(), FV->getReturnVars().end());
            }
          }
        }
      } else if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(D)) {
        /* Allocator call */
        if (isFunctionAllocator(FD->getName())) {
          bool didInsert = false;
          if (CE->getNumArgs() > 0) {
            QualType ArgTy;
            std::string FuncName = FD->getNameAsString();
            ConstAtom *A;
            A = analyzeAllocExpr(CE, CS, ArgTy, FuncName, Context);
            if (A) {
              std::string N = FD->getName(); N = "&"+N;
              ExprType = Context->getPointerType(ArgTy);
              PVConstraint *PVC =
                  new PVConstraint(ExprType, nullptr, N, Info, *Context);
              TempConstraintVars.insert(PVC);
              PVC->constrainOuterTo(CS,A,true);
              ReturnCVs.insert(PVC);
              didInsert = true;
              if (FuncName.compare("realloc") == 0) {
                // We will constrain the first arg to the return of realloc, below
                ReallocFlow = getExprConstraintVars(CE->getArg(0)->IgnoreParenImpCasts());
              }
            }
          }
          if (!didInsert)
            ReturnCVs.insert(PVConstraint::getWildPVConstraint(Info.getConstraints()));

        /* Normal function call */
        } else {
          std::set<ConstraintVariable *> CS = Info.getVariable(FD, Context);
          ConstraintVariable *J = getOnly(CS);
          /* Direct function call */
          if (FVConstraint *FVC = dyn_cast<FVConstraint>(J))
            ReturnCVs.insert(FVC->getReturnVars().begin(),
                             FVC->getReturnVars().end());
          /* Call via function pointer */
          else {
            PVConstraint *tmp = dyn_cast<PVConstraint>(J);
            assert(tmp != nullptr);
            if (FVConstraint *FVC = tmp->getFV())
              ReturnCVs.insert(FVC->getReturnVars().begin(),
                               FVC->getReturnVars().end());
            else {
              // No FVConstraint -- make WILD
              auto *TmpFV = new FVConstraint();
              TempConstraintVars.insert(TmpFV);
              ReturnCVs.insert(TmpFV);
            }
          }
        }
      } else {
        // If it ISN'T, though... what to do? How could this happen?
        llvm_unreachable("TODO");
      }

      // This is R-Value, we need to make a copy of the resulting
      // ConstraintVariables.
      std::set<ConstraintVariable *> TmpCVs;
      for (ConstraintVariable *CV : ReturnCVs) {
        ConstraintVariable *NewCV = CV->getCopy(CS);
        // Important: Do Safe_to_Wild from returnvar in this copy, which then
        //   might be assigned otherwise (Same_to_Same) to LHS
        constrainConsVarGeq(NewCV, CV, CS, nullptr, Safe_to_Wild, false, &Info);
        TmpCVs.insert(NewCV);
        // If this is realloc, constrain the first arg to flow to the return
        if (!ReallocFlow.empty()) {
          for (auto &Constraint : ReallocFlow)
            constrainConsVarGeq(NewCV, Constraint, Info.getConstraints(),
                                nullptr, Wild_to_Safe, false, &Info);

        }
      }

      return getPersistentConstraints(CE, TmpCVs);
    // e1 ? e2 : e3
    } else if (ConditionalOperator *CO = dyn_cast<ConditionalOperator>(E)) {
      std::vector<Expr *> SubExprs;
      SubExprs.push_back(CO->getLHS());
      SubExprs.push_back(CO->getRHS());
      return getAllSubExprConstraintVars(SubExprs);

    // { e1, e2, e3, ... }
    } else if (InitListExpr *ILE = dyn_cast<InitListExpr>(E)) {
      std::vector<Expr *> SubExprs = ILE->inits().vec();
      std::set<ConstraintVariable *> CVars =
          getAllSubExprConstraintVars(SubExprs);
      if(ILE->getType()->isArrayType()) {
        // Array initialization is similar AddrOf, so the same pattern is used
        // where a new indirection is added to constraint variables.
        return addAtomAll(CVars, CS.getArr(), CS);
      } else {
        // This branch should only be taken on compound literal expressions
        // with pointer type (e.g. int *a = (int*){(int*) 1}). In particular,
        // structure initialization should not reach here, as that caught by the
        // non-pointer check at the top of this method.
        assert("InitlistExpr of type other than array or pointer in "
               "getExprConstraintVars" && ILE->getType()->isPointerType());
        return CVars;
      }

    // (int[]){e1, e2, e3, ... }
    } else if (CompoundLiteralExpr *CLE = dyn_cast<CompoundLiteralExpr>(E)) {
      std::set<ConstraintVariable *> T;
      if (hasPersistentConstraints(CLE)) {
        return getPersistentConstraints(CLE, T);
      }

      std::set<ConstraintVariable *>
          Vars = getExprConstraintVars(CLE->getInitializer());

      PVConstraint *P = new PVConstraint(CLE->getType(), nullptr,
                                         CLE->getStmtClassName(), Info,
                                         *Context, nullptr);
      T = {P};

      PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(CLE, *Context);
      constrainConsVarGeq(T, Vars, Info.getConstraints(), &PL,
                          Same_to_Same, false, &Info);

      return getPersistentConstraints(CLE, T);

    // "foo"
    } else if (clang::StringLiteral *Str = dyn_cast<clang::StringLiteral>(E)) {
      std::set<ConstraintVariable *> T;
      if (hasPersistentConstraints(Str)) {
        return getPersistentConstraints(Str, T);
      }
      // If this is a string literal. i.e., "foo".
      // We create a new constraint variable and constraint it to an Nt_array.

      PVConstraint *P = new PVConstraint(Str->getType(), nullptr,
                                         Str->getStmtClassName(), Info,
                                         *Context, nullptr);
      P->constrainOuterTo(CS, CS.getNTArr()); // NB: ARR already there
      T = {P};

      return getPersistentConstraints(Str, T);

    // Checked-C temporary
    } else if (CHKCBindTemporaryExpr *CE = dyn_cast<CHKCBindTemporaryExpr>(E)) {
      return getExprConstraintVars(CE->getSubExpr());

    // Not specifically handled -- impose no constraint
    } else {
      if (Verbose) {
        llvm::errs() << "WARNING! Initialization expression ignored: ";
        E->dump(llvm::errs());
        llvm::errs() << "\n";
      }
      return std::set<ConstraintVariable *>();
    }
  }
  return std::set<ConstraintVariable *>();
}

bool ConstraintResolver::hasPersistentConstraints(clang::Expr *E) {
  std::set<ConstraintVariable *>
      &Persist = Info.getPersistentConstraintVars(E, Context);
  return !Persist.empty();
}

// Get the set of constraint variables for an expression that will persist
// between the constraint generation and rewriting pass. If the expression
// already has a set of persistent constraints, this set is returned. Otherwise,
// the set provided in the arguments is stored persistent and returned. This is
// required for correct cast insertion.
std::set<ConstraintVariable *> ConstraintResolver::getPersistentConstraints(
    clang::Expr *E, std::set<ConstraintVariable *> &Vars) {
  assert ((!hasPersistentConstraints(E) || Vars.empty()) &&
         "Persistent constraints already present.");
  std::set<ConstraintVariable *>
      &Persist = Info.getPersistentConstraintVars(E, Context);
  Persist.insert(Vars.begin(), Vars.end());
  return Persist;
}

// Collect constraint variables for Exprs int a set
std::set<ConstraintVariable *> ConstraintResolver::getAllSubExprConstraintVars(
    std::vector<Expr *> &Exprs) {

  std::set<ConstraintVariable *> AggregateCons;
  for (const auto &E : Exprs) {
    std::set<ConstraintVariable *> ECons;
    ECons = getExprConstraintVars(E);
    AggregateCons.insert(ECons.begin(), ECons.end());
  }

  return AggregateCons;
}

void ConstraintResolver::constrainLocalAssign(Stmt *TSt, Expr *LHS, Expr *RHS,
                                              ConsAction CAction) {
  PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(TSt, *Context);
  std::set<ConstraintVariable *> L = getExprConstraintVars(LHS);
  std::set<ConstraintVariable *> R = getExprConstraintVars(RHS);
  constrainConsVarGeq(L, R, Info.getConstraints(), &PL, CAction, false, &Info);

  // Only if all types are enabled and these are not pointers, then track
  // the assignment.
  if (AllTypes && !containsValidCons(L) &&
      !containsValidCons(R)) {
    BoundsKey LKey, RKey;
    auto &ABI = Info.getABoundsInfo();
    if ((resolveBoundsKey(L, LKey) ||
        ABI.tryGetVariable(LHS, *Context, LKey)) &&
        (resolveBoundsKey(R, RKey) ||
        ABI.tryGetVariable(RHS, *Context, RKey))) {
      ABI.addAssignment(LKey, RKey);
    }
  }
}

void ConstraintResolver::constrainLocalAssign(Stmt *TSt, DeclaratorDecl *D,
                                              Expr *RHS, ConsAction CAction) {
  PersistentSourceLoc PL, *PLPtr = nullptr;
  if (TSt != nullptr) {
   PL = PersistentSourceLoc::mkPSL(TSt, *Context);
   PLPtr = &PL;
  }
  // Get the in-context local constraints.
  std::set<ConstraintVariable *> V = Info.getVariable(D, Context);
  auto RHSCons = getExprConstraintVars(RHS);

  constrainConsVarGeq(V, RHSCons, Info.getConstraints(), PLPtr, CAction, false,
                      &Info);

  if (AllTypes && !containsValidCons(V) && !containsValidCons(RHSCons)) {
    BoundsKey LKey, RKey;
    auto &ABI = Info.getABoundsInfo();
    if ((resolveBoundsKey(V, LKey) ||
         ABI.tryGetVariable(D, LKey)) &&
        (resolveBoundsKey(RHSCons, RKey) ||
         ABI.tryGetVariable(RHS, *Context, RKey))) {
      ABI.addAssignment(LKey, RKey);
    }
  }
}

std::set<ConstraintVariable *> ConstraintResolver::getWildPVConstraint() {
  std::set<ConstraintVariable *> Ret;
  Ret.insert(PVConstraint::getWildPVConstraint(Info.getConstraints()));
  return Ret;
}

std::set<ConstraintVariable *> ConstraintResolver::PVConstraintFromType(QualType TypE) {
  std::set<ConstraintVariable *> Ret;
  if (TypE->isRecordType() || TypE->isArithmeticType())
    Ret.insert(PVConstraint::getNonPtrPVConstraint(Info.getConstraints()));
  else if (TypE->isPointerType())
    Ret.insert(PVConstraint::getWildPVConstraint(Info.getConstraints()));
  else
    llvm::errs() << "Warning: Returning non-base, non-wild type";
  return Ret;
}

std::set<ConstraintVariable *> ConstraintResolver::getBaseVarPVConstraint(DeclRefExpr *Decl) {
  assert(Decl->getType()->isRecordType() || Decl->getType()->isArithmeticType());
  std::set<ConstraintVariable *> Ret;
  Ret.insert(PVConstraint::getNamedNonPtrPVConstraint(Decl->getDecl()->getName(), Info.getConstraints()));
  return Ret;
}

bool
ConstraintResolver::containsValidCons(std::set<ConstraintVariable *> &CVs) {
  bool RetVal = false;
  for (auto *ConsVar : CVs) {
    if (PVConstraint *PV = dyn_cast<PVConstraint>(ConsVar)) {
      if (!PV->getCvars().empty()) {
        RetVal = true;
        break;
      }
    }
  }
  return RetVal;
}

bool ConstraintResolver::resolveBoundsKey(std::set<ConstraintVariable *> &CVs,
                                          BoundsKey &BK) {
  bool RetVal = false;
  if (CVs.size() == 1) {
    auto *OCons = getOnly(CVs);
    if (PVConstraint *PV = dyn_cast<PVConstraint>(OCons)) {
      if (PV->hasBoundsKey()) {
        BK = PV->getBoundsKey();
        RetVal = true;
      }
    }
  }
  return RetVal;
}
