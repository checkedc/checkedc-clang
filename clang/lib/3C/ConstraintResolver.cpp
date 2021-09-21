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

#include "clang/3C/ConstraintResolver.h"
#include "clang/3C/3CGlobalOptions.h"

using namespace llvm;
using namespace clang;

ConstraintResolver::~ConstraintResolver() {}

// Force all ConstraintVariables in this set to be WILD
void ConstraintResolver::constraintAllCVarsToWild(const CVarSet &CSet,
                                                  const std::string &Rsn,
                                                  Expr *AtExpr) {
  PersistentSourceLoc Psl;
  PersistentSourceLoc *PslP = nullptr;
  if (AtExpr != nullptr) {
    Psl = PersistentSourceLoc::mkPSL(AtExpr, *Context);
    PslP = &Psl;
  }
  auto &CS = Info.getConstraints();

  for (const auto &A : CSet) {
    if (PVConstraint *PVC = dyn_cast<PVConstraint>(A))
      PVC->constrainToWild(CS, Rsn, PslP);
    else {
      FVConstraint *FVC = dyn_cast<FVConstraint>(A);
      assert(FVC != nullptr);
      FVC->constrainToWild(CS, Rsn, PslP);
    }
  }
}

void ConstraintResolver::constraintCVarToWild(CVarOption CVar,
                                              const std::string &Rsn,
                                              Expr *AtExpr) {
  if (CVar.hasValue()) {
    ConstraintVariable &T = CVar.getValue();
    constraintAllCVarsToWild({&T}, Rsn, AtExpr);
  }
}

// Return a set of PVConstraints equivalent to the set given,
// but dereferenced one level down
CVarSet ConstraintResolver::handleDeref(CVarSet T) {
  CVarSet DerefVars;
  for (ConstraintVariable *CV : T) {
    PVConstraint *PVC = cast<PVConstraint>(CV);
    if (!PVC->getCvars().empty())
      DerefVars.insert(PointerVariableConstraint::derefPVConstraint(PVC));
  }
  return DerefVars;
}

// For each constraint variable either invoke addAtom to add an additional level
// of indirection (when the constraint is PVConstraint), or return the
// constraint unchanged (when the constraint is a function constraint).
CVarSet ConstraintResolver::addAtomAll(CVarSet CVS, ConstAtom *PtrTyp,
                                       Constraints &CS) {
  CVarSet Result;
  for (auto *CV : CVS) {
    if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
      Result.insert(PVConstraint::addAtomPVConstraint(PVC, PtrTyp, CS));
    } else {
      Result.insert(CV);
    }
  }
  return Result;
}

static bool getSizeOfArg(Expr *Arg, QualType &ArgTy) {
  Arg = Arg->IgnoreParenImpCasts();
  if (auto *SizeOf = dyn_cast<UnaryExprOrTypeTraitExpr>(Arg))
    if (SizeOf->getKind() == UETT_SizeOf) {
      ArgTy = SizeOf->getTypeOfArgument();
      return true;
    }
  return false;
}

// Processes E from malloc(E) to discern the pointer type this will be
static ConstAtom *analyzeAllocExpr(CallExpr *CE, Constraints &CS,
                                   QualType &ArgTy, std::string FuncName,
                                   ASTContext *Context) {
  if (FuncName.compare("calloc") == 0) {
    if (!getSizeOfArg(CE->getArg(1), ArgTy))
      return nullptr;
    // Check if first argument to calloc is 1
    int Result;
    if (evaluateToInt(CE->getArg(0), *Context, Result) && Result == 1)
      return CS.getPtr();
    // While calloc can be thought of as returning NT_ARR because it
    // initializes the allocated memory to zero, its type in the checked
    // header file is ARR so, we cannot safely return NT_ARR here.
    return CS.getArr();
  }

  ConstAtom *Ret = CS.getPtr();
  Expr *E;
  if (std::find(_3COpts.AllocatorFunctions.begin(),
                _3COpts.AllocatorFunctions.end(),
                FuncName) != _3COpts.AllocatorFunctions.end() ||
      FuncName.compare("malloc") == 0)
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
    Ret = CS.getArr();
    Exprs.insert(B->getLHS());
    Exprs.insert(B->getRHS());
  } else
    Exprs.insert(E);

  // Look for sizeof(X); return Arr or Ptr if found
  for (Expr *Ex : Exprs)
    if (getSizeOfArg(Ex, ArgTy))
      return Ret;
  return nullptr;
}

CVarSet ConstraintResolver::getInvalidCastPVCons(CastExpr *E) {
  QualType DstType = E->getType();
  QualType SrcType = E->getSubExpr()->getType();

  auto *P = new PVConstraint(E, Info, *Context);
  PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(E, *Context);
  std::string Rsn =
      "Cast from " + SrcType.getAsString() + " to " + DstType.getAsString();
  P->constrainToWild(Info.getConstraints(), Rsn, &PL);
  return {P};
}

inline CSetBkeyPair pairWithEmptyBkey(const CVarSet &Vars) {
  BKeySet EmptyBSet;
  EmptyBSet.clear();
  return std::make_pair(Vars, EmptyBSet);
}

// Get the return type of the function from the TypeVars, that is, from
// the local instantiation of a generic function. Or the regular return
// constraint if it's not generic
ConstraintVariable *localReturnConstraint(
    FVConstraint *FV,
    ProgramInfo::CallTypeParamBindingsT TypeVars,
    Constraints &CS,
    ProgramInfo &Info) {
  int TyVarIdx = FV->getExternalReturn()->getGenericIndex();
  // Check if local type vars are available
  if (TypeVars.find(TyVarIdx) != TypeVars.end() &&
      TypeVars[TyVarIdx].isConsistent()) {
    ConstraintVariable *CV = TypeVars[TyVarIdx].getConstraint(
            Info.getConstraints().getVariables());
    if (FV->getExternalReturn()->hasBoundsKey())
      CV->setBoundsKey(FV->getExternalReturn()->getBoundsKey());
    return CV;
  } else {
    return FV->getExternalReturn();
  }
}

// Returns a pair of set of ConstraintVariables and set of BoundsKey
// after evaluating the expression E. Will explore E recursively, but will
// ignore parts of it that do not contribute to the final result.
CSetBkeyPair ConstraintResolver::getExprConstraintVars(Expr *E) {
  CSetBkeyPair EmptyCSBKeySet;
  BKeySet EmptyBSet;
  auto &ABI = Info.getABoundsInfo();
  if (E != nullptr) {
    auto &CS = Info.getConstraints();
    QualType TypE = E->getType();
    E = E->IgnoreParens();

    // Non-pointer (int, char, etc.) types have a special base PVConstraint.
    if (isNonPtrType(TypE)) {
      if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
        // If we have a DeclRef, the PVC can get a meaningful name
        return pairWithEmptyBkey(getBaseVarPVConstraint(DRE));
      }
      // Fetch the context sensitive bounds key.
      return std::make_pair(pvConstraintFromType(TypE),
                            ABI.getCtxSensFieldBoundsKey(E, Context, Info));
    }
    // NULL
    // Special handling for casts of null is required to enable rewriting
    // statements such as int *x = (int*) 0. If this was handled as a
    // normal null expression, the cast would never be visited.
    if (!isa<ExplicitCastExpr>(E) && isNULLExpression(E, *Context)) {
      return EmptyCSBKeySet;
    }
    // variable (x)
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
      CVarOption CV = Info.getVariable(DRE->getDecl(), Context);
      assert("Declaration without constraint variable?" && CV.hasValue());
      return pairWithEmptyBkey({&CV.getValue()});
    }
    // x.f
    if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
      CVarOption CV = Info.getVariable(ME->getMemberDecl(), Context);
      assert("Declaration without constraint variable?" && CV.hasValue());
      CVarSet MECSet = {&CV.getValue()};
      // Get Context sensitive bounds key for field access.
      return std::make_pair(MECSet,
                            ABI.getCtxSensFieldBoundsKey(ME, Context, Info));
    }
    // Checked-C temporary
    if (CHKCBindTemporaryExpr *CE = dyn_cast<CHKCBindTemporaryExpr>(E)) {
      return getExprConstraintVars(CE->getSubExpr());
    }

    // Apart from the above expressions constraints for all the other
    // expressions can be cached.
    // First, check if the expression has constraints that are cached?
    if (Info.hasPersistentConstraints(E, Context))
      return Info.getPersistentConstraints(E, Context);

    CSetBkeyPair Ret = EmptyCSBKeySet;
    // Implicit cast, e.g., T* from T[] or int (*)(int) from int (int),
    // but also weird int->int * conversions (and back).
    if (ImplicitCastExpr *IE = dyn_cast<ImplicitCastExpr>(E)) {
      // ImplicitCastExpr is a compiler generated AST node, so we would not
      // typically want to depend on its source location being unique, but
      // their constraint var sets are stored in a separate map, avoiding
      // source location collision with other expressions.
      QualType SubTypE = IE->getSubExpr()->getType();
      auto CVs = getExprConstraintVars(IE->getSubExpr());
      // If TypE is a pointer type, and the cast is unsafe, return WildPtr.
      if (TypE->isPointerType() &&
          !(SubTypE->isFunctionType() || SubTypE->isArrayType() ||
            SubTypE->isVoidPointerType()) &&
          !isCastSafe(TypE, SubTypE)) {
        CVarSet WildCVar = getInvalidCastPVCons(IE);
        constrainConsVarGeq(CVs.first, WildCVar, CS, nullptr, Safe_to_Wild,
                            false, &Info);
        Ret = std::make_pair(WildCVar, CVs.second);
      } else {
        // Else, return sub-expression's result.
        Ret = CVs;
      }
    } else if (ExplicitCastExpr *ECE = dyn_cast<ExplicitCastExpr>(E)) {
      assert(ECE->getType() == TypE);
      Expr *TmpE = ECE->getSubExpr();
      // Is cast internally safe? Return WILD if not.
      // If the cast is NULL, it will otherwise seem invalid, but we want to
      // handle it as usual so the type in the cast can be rewritten.
      if (!isNULLExpression(ECE, *Context) && TypE->isPointerType() &&
          !isCastSafe(TypE, TmpE->getType()) && !isCastofGeneric(ECE)) {
        CVarSet Vars = getExprConstraintVarsSet(TmpE);
        Ret = pairWithEmptyBkey(getInvalidCastPVCons(ECE));
        constrainConsVarGeq(Vars, Ret.first, CS, nullptr, Safe_to_Wild, false,
                            &Info);
        // NB: Expression ECE itself handled in
        // ConstraintBuilder::FunctionVisitor.
      } else {
        CVarSet Vars = getExprConstraintVarsSet(TmpE);
        // PVConstraint introduced for explicit cast so they can be rewritten.
        // Pretty much the same idea as CompoundLiteralExpr.
        PVConstraint *P = getRewritablePVConstraint(ECE);
        Ret = pairWithEmptyBkey({P});
        // ConstraintVars for TmpE when ECE is NULL will be WILD, so
        // constraining GEQ these vars would be the cast always be WILD.
        if (!isNULLExpression(ECE, *Context)) {
          PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(ECE, *Context);
          constrainConsVarGeq(P, Vars, Info.getConstraints(), &PL, Same_to_Same,
                              false, &Info);
        }
      }
    }
    // x = y, x+y, x+=y, etc.
    else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
      switch (BO->getOpcode()) {
        // Assignment, comma operators; only care about LHS.
      case BO_Assign:
      case BO_AddAssign:
      case BO_SubAssign:
        Ret = getExprConstraintVars(BO->getLHS());
        break;
      case BO_Comma:
        Ret = getExprConstraintVars(BO->getRHS());
        break;
        // Possible pointer arithmetic: Could be LHS or RHS.
      case BO_Add:
      case BO_Sub:
        if (BO->getLHS()->getType()->isPointerType())
          Ret = getExprConstraintVars(BO->getLHS());
        else if (BO->getRHS()->getType()->isPointerType())
          Ret = getExprConstraintVars(BO->getRHS());
        else
          Ret = pairWithEmptyBkey(pvConstraintFromType(TypE));
        break;
        // Pointer-to-member ops unsupported.
      case BO_PtrMemD:
      case BO_PtrMemI:
        assert(false && "Bogus pointer-to-member operator");
        break;
        // Bit-shift/arithmetic/assign/comp operators
        // Ret = ints; do nothing.
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
        Ret = pairWithEmptyBkey(pvConstraintFromType(TypE));
        break;
      }
    }
    // x[e]
    else if (ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(E)) {
      CSetBkeyPair T = getExprConstraintVars(ASE->getBase());
      CVarSet Tmp = handleDeref(T.first);
      T.first.swap(Tmp);
      Ret = T;
    }
    // ++e, &e, *e, etc.
    else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
      Expr *UOExpr = UO->getSubExpr();
      switch (UO->getOpcode()) {
        // &e
        // C99 6.5.3.2: "The operand of the unary & operator shall be
        // either a function designator, the result of a [] or
        // unary * operator, or an lvalue that designates an object that is
        // not a bit-field and is not declared with the register
        // storage-class specifier."
      case UO_AddrOf: {

        UOExpr = UOExpr->IgnoreParenImpCasts();
        // Taking the address of a dereference is a NoOp, so the constraint
        // vars for the subexpression can be passed through.
        // FIXME: We've dumped implicit casts on UOEXpr; restore?
        if (UnaryOperator *SubUO = dyn_cast<UnaryOperator>(UOExpr)) {
          if (SubUO->getOpcode() == UO_Deref)
            Ret = getExprConstraintVars(SubUO->getSubExpr());
          // else, fall through
        } else if (ArraySubscriptExpr *ASE =
                       dyn_cast<ArraySubscriptExpr>(UOExpr)) {
          Ret = getExprConstraintVars(ASE->getBase());
        } else {
          CSetBkeyPair T = getExprConstraintVars(UOExpr);
          assert("Empty constraint vars in AddrOf!" && !T.first.empty());
          // CheckedC prohibits taking the address of a variable with bounds. To
          // avoid doing this, constrain the target of AddrOf expressions to
          // PTR. This prevents it from solving to either ARR or NTARR. CheckedC
          // does permit taking the address of an _Array_ptr when the array
          // pointer has no declared bounds. With this constraint added however,
          // 3C will not generate such code.
          for (auto *CV : T.first)
            if (auto *PCV = dyn_cast<PVConstraint>(CV))
              // On the other hand, CheckedC does let you take the address of
              // constant sized arrays.
              if (!PCV->isConstantArr())
                PCV->constrainOuterTo(CS, CS.getPtr(), true);
          // Add a VarAtom to UOExpr's PVConstraint, for &.
          Ret = std::make_pair(addAtomAll(T.first, CS.getPtr(), CS), T.second);
        }
        break;
      }
        // *e
      case UO_Deref: {
        // We are dereferencing, so don't assign to LHS
        CSetBkeyPair T = getExprConstraintVars(UOExpr);
        Ret = std::make_pair(handleDeref(T.first), T.second);
        break;
      }
        /* Operations on lval; if pointer, just process that */
        // e++, e--, ++e, --e
      case UO_PostInc:
      case UO_PostDec:
      case UO_PreInc:
      case UO_PreDec:
        Ret = getExprConstraintVars(UOExpr);
        break;
        /* Integer operators */
        // +e, -e, ~e
      case UO_Plus:
      case UO_Minus:
      case UO_LNot:
      case UO_Not:
        Ret = pairWithEmptyBkey(pvConstraintFromType(TypE));
        break;
      case UO_Coawait:
      case UO_Real:
      case UO_Imag:
      case UO_Extension:
        assert(false && "Unsupported unary operator");
        break;
      }
    }
    // f(e1,e2, ...)
    else if (CallExpr *CE = dyn_cast<CallExpr>(E)) {
      // Call expression should always get out-of context constraint variable.
      CVarSet ReturnCVs;
      BKeySet ReturnBSet = EmptyBSet;

      ProgramInfo::CallTypeParamBindingsT TypeVars;
      if (Info.hasTypeParamBindings(CE, Context))
        TypeVars = Info.getTypeParamBindings(CE, Context);

      // Here, we need to look up the target of the call and return the
      // constraints for the return value of that function.
      QualType ExprType = E->getType();
      Decl *D = CE->getCalleeDecl();
      CVarSet ReallocFlow;
      bool IsAllocator = false;
      if (D == nullptr) {
        // There are a few reasons that we couldn't get a decl. For example,
        // the call could be done through an array subscript.
        Expr *CalledExpr = CE->getCallee();
        CSetBkeyPair Tmp = getExprConstraintVars(CalledExpr);
        ReturnBSet = Tmp.second;

        for (ConstraintVariable *C : Tmp.first) {
          if (FVConstraint *FV = dyn_cast<FVConstraint>(C)) {
            ReturnCVs.insert(localReturnConstraint(FV,TypeVars,CS,Info));
          } else if (PVConstraint *PV = dyn_cast<PVConstraint>(C)) {
            if (FVConstraint *FV = PV->getFV())
              ReturnCVs.insert(localReturnConstraint(FV,TypeVars,CS,Info));
          }
        }
      } else if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(D)) {
        /* Allocator call */
        if (isFunctionAllocator(std::string(FD->getName()))) {
          bool DidInsert = false;
          IsAllocator = true;
          if (TypeVars.find(0) != TypeVars.end() &&
              TypeVars[0].isConsistent()) {
            ConstraintVariable *CV = TypeVars[0].getConstraint(
                       Info.getConstraints().getVariables());
            ReturnCVs.insert(CV);
            DidInsert = true;
          } else if (CE->getNumArgs() > 0) {
            QualType ArgTy;
            std::string FuncName = FD->getNameAsString();
            ConstAtom *A = analyzeAllocExpr(CE, CS, ArgTy, FuncName, Context);
            if (A) {
              std::string N(FD->getName());
              N = "&" + N;
              ExprType = Context->getPointerType(ArgTy);
              PVConstraint *PVC = new PVConstraint(ExprType, nullptr, N, Info,
                                                   *Context, nullptr, 0);
              PVC->constrainOuterTo(CS, A, true);
              ReturnCVs.insert(PVC);
              DidInsert = true;
              if (FuncName.compare("realloc") == 0) {
                // We will constrain the first arg to the return of
                // realloc, below
                ReallocFlow =
                    getExprConstraintVars(CE->getArg(0)->IgnoreParenImpCasts())
                        .first;
              }
            }
          }
          if (!DidInsert) {
            std::string Rsn = "Unsafe call to allocator function.";
            PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(CE, *Context);
            ReturnCVs.insert(PVConstraint::getWildPVConstraint(
                Info.getConstraints(), Rsn, &PL));
          }

          /* Normal function call */
        } else {
          CVarOption CV = Info.getVariable(FD, Context);
          assert(CV.hasValue() && "Function without constraint variable.");
          /* Direct function call */
          if (FVConstraint *FVC = dyn_cast<FVConstraint>(&CV.getValue()))
            ReturnCVs.insert(localReturnConstraint(FVC,TypeVars,CS,Info));
          /* Call via function pointer */
          else {
            PVConstraint *Tmp = dyn_cast<PVConstraint>(&CV.getValue());
            assert(Tmp != nullptr);
            if (FVConstraint *FVC = Tmp->getFV())
              ReturnCVs.insert(localReturnConstraint(FVC,TypeVars,CS,Info));
            else {
              // No FVConstraint -- make WILD.
              std::string Rsn = "Can't get return variable of function call.";
              PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(CE, *Context);
              ReturnCVs.insert(PVConstraint::getWildPVConstraint(CS, Rsn, &PL));
            }
          }
        }
      } else {
        // If it ISN'T, though... what to do? How could this happen?
        llvm_unreachable("TODO");
      }

      // This is R-Value, we need to make a copy of the resulting
      // ConstraintVariables.
      CVarSet TmpCVs;
      for (ConstraintVariable *CV : ReturnCVs) {
        ConstraintVariable *NewCV;
        auto *PCV = dyn_cast<PVConstraint>(CV);
        if (!IsAllocator) {
          if (PCV && PCV->isOriginallyChecked()) {
            // Copying needs to be done differently if the constraint variable
            // had a checked type in the input program because the constraint
            // variables contain constant atoms that are reused by the copy
            // constructor.
            auto *NewPCV =
                new PVConstraint(CE->getType(), nullptr, PCV->getName(), Info,
                                 *Context, nullptr, PCV->getGenericIndex());
            NewCV = NewPCV;
            if (PCV->hasBoundsKey())
              NewCV->setBoundsKey(PCV->getBoundsKey());
          } else {
            NewCV = CV->getCopy(CS);
          }
        } else {
          // Allocator functions are treated specially, so they do not have
          // separate parameter and argument return variables.
          NewCV = CV;
        }

        auto PSL = PersistentSourceLoc::mkPSL(CE, *Context);
        // Make the bounds key context sensitive.
        if (NewCV->hasBoundsKey()) {
          auto CSensBKey =
              ABI.getCtxSensCEBoundsKey(PSL, NewCV->getBoundsKey());
          NewCV->setBoundsKey(CSensBKey);
        }
        if (NewCV != CV) {
          // If the call is in a macro, use Same_to_Same to force checked type
          // equality and avoid ever needing to insert a cast inside a macro.
          ConsAction CA = Rewriter::isRewritable(CE->getExprLoc())
                              ? Safe_to_Wild
                              : Same_to_Same;
          constrainConsVarGeq(NewCV, CV, CS, &PSL, CA, false, &Info);
        }
        TmpCVs.insert(NewCV);
        // If this is realloc, constrain the first arg to flow to the return
        if (!ReallocFlow.empty()) {
          constrainConsVarGeq(NewCV, ReallocFlow, Info.getConstraints(), &PSL,
                              Wild_to_Safe, false, &Info);
        }
      }
      Ret = std::make_pair(TmpCVs, ReturnBSet);
    }
    // e1 ? e2 : e3
    else if (ConditionalOperator *CO = dyn_cast<ConditionalOperator>(E)) {
      std::vector<Expr *> SubExprs;
      SubExprs.push_back(CO->getLHS());
      SubExprs.push_back(CO->getRHS());
      Ret = getAllSubExprConstraintVars(SubExprs);
    }
    // { e1, e2, e3, ... }
    else if (InitListExpr *ILE = dyn_cast<InitListExpr>(E)) {
      std::vector<Expr *> SubExprs = ILE->inits().vec();
      CSetBkeyPair CVars = getAllSubExprConstraintVars(SubExprs);
      if (ILE->getType()->isArrayType()) {
        // Array initialization is similar AddrOf, so the same pattern is
        // used where a new indirection is added to constraint variables.
        Ret = std::make_pair(addAtomAll(CVars.first, CS.getArr(), CS),
                             CVars.second);
      } else {
        // This branch should only be taken on compound literal expressions
        // with pointer type (e.g. int *a = (int*){(int*) 1}).
        // In particular, structure initialization should not reach here,
        // as that caught by the non-pointer check at the top of this
        // method.
        assert("InitlistExpr of type other than array or pointer in "
               "getExprConstraintVars" &&
               ILE->getType()->isPointerType());
        Ret = CVars;
      }
    }
    // (int[]){e1, e2, e3, ... }
    else if (CompoundLiteralExpr *CLE = dyn_cast<CompoundLiteralExpr>(E)) {
      CSetBkeyPair Vars = getExprConstraintVars(CLE->getInitializer());

      PVConstraint *P = getRewritablePVConstraint(CLE);

      PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(CLE, *Context);
      constrainConsVarGeq(P, Vars.first, Info.getConstraints(), &PL,
                          Same_to_Same, false, &Info);

      CVarSet T = {P};
      Ret = std::make_pair(T, Vars.second);
    }
    // "foo"
    else if (clang::StringLiteral *Str = dyn_cast<clang::StringLiteral>(E)) {
      CVarSet T;
      // If this is a string literal. i.e., "foo".
      // We create a new constraint variable and constraint it to an Nt_array.

      PVConstraint *P = new PVConstraint(Str, Info, *Context);
      P->constrainOuterTo(CS, CS.getNTArr()); // NB: ARR already there.

      BoundsKey TmpKey = ABI.getRandomBKey();
      P->setBoundsKey(TmpKey);

      BoundsKey CBKey = ABI.getConstKey(Str->getByteLength());
      ABounds *NB = new CountBound(CBKey);
      ABI.replaceBounds(TmpKey, Declared, NB);

      T = {P};

      Ret = pairWithEmptyBkey(T);
    } else if (StmtExpr *SE = dyn_cast<StmtExpr>(E)) {
      CVarSet T;
      // Retrieve the last "thing" returned by the block.
      Stmt *Res = SE->getSubStmt()->getStmtExprResult();
      if (Expr *ESE = dyn_cast<Expr>(Res)) {
        return getExprConstraintVars(ESE);
      }
    } else if (VAArgExpr *VarArg = dyn_cast<VAArgExpr>(E)) {
      // Use of VarArg parameters are assumed to be unsafe even though CheckedC
      // will accept them with checked pointer types. If we want to support
      // VarArgs with checked pointer types, we can remove the constraint to
      // WILD here. We would then need to update TypeExprRewriter to rewrite the
      // type in these expression.
      auto *P = new PVConstraint(VarArg, Info, *Context);
      PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(E, *Context);
      std::string Rsn = "Accessing VarArg parameter";
      P->constrainToWild(Info.getConstraints(), Rsn, &PL);
      Ret = pairWithEmptyBkey({P});
    } else {
      if (_3COpts.Verbose) {
        llvm::errs() << "WARNING! Initialization expression ignored: ";
        E->dump(llvm::errs(), *Context);
        llvm::errs() << "\n";
      }
    }
    Info.storePersistentConstraints(E, Ret, Context);
    return Ret;
  }
  return EmptyCSBKeySet;
}

CVarSet ConstraintResolver::getExprConstraintVarsSet(Expr *E) {
  return getExprConstraintVars(E).first;
}

// Collect constraint variables for Exprs int a set.
CSetBkeyPair
ConstraintResolver::getAllSubExprConstraintVars(std::vector<Expr *> &Exprs) {

  CVarSet AggregateCons;
  BKeySet AggregateBKeys;
  for (const auto &E : Exprs) {
    CSetBkeyPair ECons;
    ECons = getExprConstraintVars(E);
    AggregateCons.insert(ECons.first.begin(), ECons.first.end());
    AggregateBKeys.insert(ECons.second.begin(), ECons.second.end());
  }

  return std::make_pair(AggregateCons, AggregateBKeys);
}

void ConstraintResolver::constrainLocalAssign(Stmt *TSt, Expr *LHS, Expr *RHS,
                                              ConsAction CAction) {
  PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(TSt, *Context);
  CSetBkeyPair L = getExprConstraintVars(LHS);
  CSetBkeyPair R = getExprConstraintVars(RHS);
  bool HandleBoundsKey = L.second.empty() && R.second.empty();
  constrainConsVarGeq(L.first, R.first, Info.getConstraints(), &PL, CAction,
                      false, &Info, HandleBoundsKey);

  // Handle pointer arithmetic.
  auto &ABI = Info.getABoundsInfo();
  ABI.handlePointerAssignment(TSt, LHS, RHS, Context, this);

  // Only if all types are enabled and these are not pointers, then track
  // the assignment.
  if (_3COpts.AllTypes) {
    if ((!containsValidCons(L.first) && !containsValidCons(R.first)) ||
        !HandleBoundsKey) {
      ABI.handleAssignment(LHS, L.first, L.second, RHS, R.first, R.second,
                           Context, this);
    }
  }
}

void ConstraintResolver::constrainLocalAssign(Stmt *TSt, DeclaratorDecl *D,
                                              Expr *RHS, ConsAction CAction,
                                              bool IgnoreBnds) {
  PersistentSourceLoc PL, *PLPtr = nullptr;
  if (TSt != nullptr) {
    PL = PersistentSourceLoc::mkPSL(TSt, *Context);
    PLPtr = &PL;
  }
  // Get the in-context local constraints.
  CVarOption V = Info.getVariable(D, Context);
  auto RHSCons = getExprConstraintVars(RHS);
  bool HandleBoundsKey = IgnoreBnds || RHSCons.second.empty();

  if (V.hasValue())
    constrainConsVarGeq(&V.getValue(), RHSCons.first, Info.getConstraints(),
                        PLPtr, CAction, false, &Info, HandleBoundsKey);
  if (_3COpts.AllTypes && !IgnoreBnds) {
    if (!HandleBoundsKey || (!(V.hasValue() && isValidCons(&V.getValue())) &&
                             !containsValidCons(RHSCons.first))) {
      auto &ABI = Info.getABoundsInfo();
      ABI.handleAssignment(D, V, RHS, RHSCons.first, RHSCons.second, Context,
                           this);
    }
  }
}

bool ConstraintResolver::isNonPtrType(QualType &TE) {
  return TE->isRecordType() || TE->isArithmeticType() || TE->isVectorType();
}

CVarSet ConstraintResolver::pvConstraintFromType(QualType TypE) {
  assert("Pointer type CVs should be obtained through getExprConstraintVars." &&
         !TypE->isPointerType());
  CVarSet Ret;
  if (isNonPtrType(TypE))
    Ret.insert(PVConstraint::getNonPtrPVConstraint(Info.getConstraints()));
  else
    llvm::errs() << "Warning: Returning non-base, non-wild type";
  return Ret;
}

CVarSet ConstraintResolver::getBaseVarPVConstraint(DeclRefExpr *Decl) {
  if (Info.hasPersistentConstraints(Decl, Context))
    return Info.getPersistentConstraintsSet(Decl, Context);

  auto T = Decl->getType();
  assert(isNonPtrType(T));

  CVarSet Ret;
  auto DN = Decl->getDecl()->getName();
  Ret.insert(
      PVConstraint::getNamedNonPtrPVConstraint(DN, Info.getConstraints()));
  Info.storePersistentConstraints(Decl, Ret, Context);
  return Ret;
}

CVarSet ConstraintResolver::getCalleeConstraintVars(CallExpr *CE) {
  CVarSet FVCons;
  Decl *D = CE->getCalleeDecl();
  if (isa_and_nonnull<FunctionDecl>(D) || isa_and_nonnull<DeclaratorDecl>(D)) {
    CVarOption CV = Info.getVariable(D, Context);
    if (CV.hasValue())
      FVCons.insert(&CV.getValue());
  } else {
    Expr *CalledExpr = CE->getCallee();
    FVCons = getExprConstraintVarsSet(CalledExpr);
  }
  return FVCons;
}

// This serves as an exception to the unsafety of casting from void*.
// In most cases, CheckedC can handle generic casts, so we can ignore them.
// CheckedC can't handle allocators without checked headers, so we add them
// to this exception for when we're dealing with small examples.
bool ConstraintResolver::isCastofGeneric(CastExpr *C) {
  Expr *SE = C->getSubExpr();
  if (CHKCBindTemporaryExpr *CE = dyn_cast<CHKCBindTemporaryExpr>(SE))
    SE = CE->getSubExpr();
  if (auto *CE = dyn_cast_or_null<CallExpr>(SE)) {
    // Check for built-in allocators, in case the standard headers are not used.
    // This is a required exception, because allocators return void*, and casts
    // from it are unsafe. We assume the cast is appropriate. With checked
    // headers clang can figure out if it is safe.
    if (auto *DD = dyn_cast_or_null<DeclaratorDecl>(CE->getCalleeDecl())) {
      std::string Name = DD->getNameAsString();
      if (isFunctionAllocator(Name))
        return true;
    }
    // Check for a generic function call.
    CVarSet CVS = getCalleeConstraintVars(CE);
    // If there are multiple constraints in the expression we
    // can't guarantee safety, so only return true if the
    // cast is directly on a function call.
    if (CVS.size() == 1) {
      if (auto *FVC = dyn_cast<FVConstraint>(*CVS.begin())) {
        return FVC->getGenericParams() > 0;
      }
    }
  }
  return false;
}

// Construct a PVConstraint for an expression that can safely be used when
// rewriting the expression later on. This is done by making the constraint WILD
// if the expression is inside a macro.
PVConstraint *ConstraintResolver::getRewritablePVConstraint(Expr *E) {
  PVConstraint *P = new PVConstraint(E, Info, *Context);
  auto PSL = PersistentSourceLoc::mkPSL(E, *Context);
  Info.constrainWildIfMacro(P, E->getExprLoc(), &PSL);
  return P;
}

bool ConstraintResolver::containsValidCons(const CVarSet &CVs) {
  for (auto *ConsVar : CVs)
    if (isValidCons(ConsVar))
      return true;
  return false;
}

bool ConstraintResolver::isValidCons(ConstraintVariable *CV) {
  if (PVConstraint *PV = dyn_cast<PVConstraint>(CV))
    return !PV->getCvars().empty();
  return false;
}

bool ConstraintResolver::resolveBoundsKey(const CVarSet &CVs, BoundsKey &BK) {
  if (CVs.size() == 1) {
    auto *OCons = getOnly(CVs);
    return resolveBoundsKey(*OCons, BK);
  }
  return false;
}

bool ConstraintResolver::resolveBoundsKey(CVarOption CVOpt, BoundsKey &BK) {
  if (CVOpt.hasValue()) {
    ConstraintVariable &CV = CVOpt.getValue();
    if (PVConstraint *PV = dyn_cast<PVConstraint>(&CV))
      if (PV->hasBoundsKey()) {
        BK = PV->getBoundsKey();
        return true;
      }
  }
  return false;
}

bool ConstraintResolver::canFunctionBeSkipped(const std::string &FN) {
  return FN == "realloc";
}
