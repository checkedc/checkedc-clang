//===---- CGDynamicCheck.cpp - Emit LLVM Code for Checked C Dynamic Checks -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Checked C Dynamic Checks as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "llvm/ADT/Statistic.h"

using namespace clang;
using namespace CodeGen;
using namespace llvm;

#define DEBUG_TYPE "DynamicCheckCodeGen"

namespace {
  STATISTIC(NumDynamicChecksElided, "The # of dynamic checks elided (due to constant folding)");
  STATISTIC(NumDynamicChecksInserted, "The # of dynamic checks inserted");

  STATISTIC(NumDynamicChecksExplicit, "The # of dynamic _Dynamic_check(cond) checks found");
  STATISTIC(NumDynamicChecksNonNull, "The # of dynamic non-null checks found");
  STATISTIC(NumDynamicChecksOverflow, "The # of dynamic overflow checks found");
  STATISTIC(NumDynamicChecksRange, "The # of dynamic bounds checks found");
}

//
// Expression-specific dynamic check insertion
//

void CodeGenFunction::EmitExplicitDynamicCheck(const Expr *Condition) {
  if (!getLangOpts().CheckedC)
    return;

  ++NumDynamicChecksExplicit;

  // Emit Check
  Value *ConditionVal = EvaluateExprAsBool(Condition);
  EmitDynamicCheckBlocks(ConditionVal);
}

//
// General Functions for inserting dynamic checks
//

void CodeGenFunction::EmitDynamicNonNullCheck(const Address BaseAddr, const QualType BaseTy) {
  if (!getLangOpts().CheckedC)
    return;

  if (!(BaseTy->isCheckedPointerType() || BaseTy->isCheckedArrayType()))
    return;

  ++NumDynamicChecksNonNull;

  Value *ConditionVal = Builder.CreateIsNotNull(BaseAddr.getPointer(), "_Dynamic_check.non_null");
  EmitDynamicCheckBlocks(ConditionVal);
}

// TODO: This is currently unused. It may never be used.
void CodeGenFunction::EmitDynamicOverflowCheck(const Address BaseAddr, const QualType BaseTy, const Address PtrAddr) {
  if (!getLangOpts().CheckedC)
    return;

  ++NumDynamicChecksOverflow;

  // EmitDynamicCheckBlocks(Condition);
}

void CodeGenFunction::EmitDynamicBoundsCheck(const Address PtrAddr, const BoundsExpr *Bounds) {
  if (!getLangOpts().CheckedC)
    return;

  if (!Bounds)
    return;

  if (Bounds->isAny() || Bounds->isInvalid())
    return;

  // We can only generate the check if we have the bounds as a range.
  if (!isa<RangeBoundsExpr>(Bounds)) {
    llvm_unreachable("Can Only Emit Dynamic Bounds Check For RangeBounds Exprs");
    return;
  }

  const RangeBoundsExpr *BoundsRange = dyn_cast<RangeBoundsExpr>(Bounds);

  ++NumDynamicChecksRange;

  // Emit the code to generate the pointer values
  Address Lower = EmitPointerWithAlignment(BoundsRange->getLowerExpr());
  Address Upper = EmitPointerWithAlignment(BoundsRange->getUpperExpr());

  // Emit the address as an int
  Value *PtrInt = Builder.CreatePtrToInt(PtrAddr.getPointer(), IntPtrTy, "_Dynamic_check.addr");

  // Make the lower check
  Value *LowerInt = Builder.CreatePtrToInt(Lower.getPointer(), IntPtrTy, "_Dynamic_check.lower");
  Value *LowerChk = Builder.CreateICmpULE(LowerInt, PtrInt, "_Dynamic_check.lower_cmp");

  // Make the upper check
  Value* UpperInt = Builder.CreatePtrToInt(Upper.getPointer(), IntPtrTy, "_Dynamic_check.upper");
  Value* UpperChk = Builder.CreateICmpULT(PtrInt, UpperInt, "_Dynamic_check.upper_cmp");

  // Emit both checks
  EmitDynamicCheckBlocks(Builder.CreateAnd(LowerChk, UpperChk, "_Dynamic_check.range"));
}

void CodeGenFunction::EmitDynamicBoundsCheck(const Address BaseAddr,
                                             const BoundsExpr *CastBounds,
                                             const BoundsExpr *SubExprBounds) {
  if (!getLangOpts().CheckedC)
    return;

  if (!SubExprBounds || !CastBounds)
    return;

  if (SubExprBounds->isAny() || SubExprBounds->isInvalid())
    return;

  // SubExprBounds can be Any by inference but CastBounds can't be Any
  assert(!CastBounds->isAny());
  if (CastBounds->isInvalid())
    return;

  // We can only generate the check if we have the bounds as a range.
  if (!isa<RangeBoundsExpr>(SubExprBounds) ||
      !isa<RangeBoundsExpr>(CastBounds)) {
    llvm_unreachable(
        "Can Only Emit Dynamic Bounds Check For RangeBounds Exprs");
    return;
  }

  const RangeBoundsExpr *SubRange = dyn_cast<RangeBoundsExpr>(SubExprBounds);
  const RangeBoundsExpr *CastRange = dyn_cast<RangeBoundsExpr>(CastBounds);

  ++NumDynamicChecksRange;

  // Dynamic_check(Base == NULL || (Lower <= CastLower && CastUpper <= Upper))
  // Emit code blocks as follows:
  // if (Base == NULL) {...}
  // else {
  // if (Lower <= CastLower && CastUpper <= Upper) {...}
  // else { trap(); llvm_unreachable(); }
  // }

  Value *Cond1 =
            Builder.CreateIsNull(BaseAddr.getPointer(), "_Dynamic_check.null");

  // Constant Folding:
  // If we have generated a constant condition, and the condition is true,
  // then the check will always pass and we can elide it.
  if (const ConstantInt *ConditionConstant = dyn_cast<ConstantInt>(Cond1)) {
    if (ConditionConstant->isOne()) {
      ++NumDynamicChecksElided;
      return;
    }
  }

  ++NumDynamicChecksInserted;

  BasicBlock *Begin, *DyCkSuccess, *DyCkFail, *DyCkFallThrough;
  Begin = Builder.GetInsertBlock();
  DyCkSuccess = createBasicBlock("_Dynamic_check.succeeded");
  DyCkFallThrough = createBasicBlock("_Dynamic_check.fallthrough", this->CurFn);
  DyCkFail = createBasicBlock("_Dynamic_check.failed", this->CurFn);

  Builder.SetInsertPoint(DyCkFallThrough);
  // SubRange - bounds(lb, ub) vs CastRange - bounds(castlb, castub)
  // Dynamic_check(lb <= castlb && castub <= ub)

  // Emit the code to generate the pointer values
  Address Lower = EmitPointerWithAlignment(SubRange->getLowerExpr());
  Address Upper = EmitPointerWithAlignment(SubRange->getUpperExpr());

  Value *LowerInt = Builder.CreatePtrToInt(Lower.getPointer(), IntPtrTy,
                                           "_Dynamic_check.lower");
  Value *UpperInt = Builder.CreatePtrToInt(Upper.getPointer(), IntPtrTy,
                                           "_Dynamic_check.upper");

  Address CastLower = EmitPointerWithAlignment(CastRange->getLowerExpr());
  Address CastUpper = EmitPointerWithAlignment(CastRange->getUpperExpr());

  Value *CastLowerInt = Builder.CreatePtrToInt(CastLower.getPointer(), IntPtrTy,
                                               "_Dynamic_check.castlower");
  Value *CastUpperInt = Builder.CreatePtrToInt(CastUpper.getPointer(), IntPtrTy,
                                               "_Dynamic_check.castupper");

  // Make the lower check (Lower <= CastLower)
  Value *LowerChk =
      Builder.CreateICmpULE(LowerInt, CastLowerInt, "_Dynamic_check.lower_cmp");

  // Make the upper check (CastUpper <= Upper)
  Value *UpperChk =
      Builder.CreateICmpULE(CastUpperInt, UpperInt, "_Dynamic_check.upper_cmp");

  Value *Cond2 = Builder.CreateAnd(LowerChk, UpperChk, "_Dynamic_check.range");
  Builder.CreateCondBr(Cond2, DyCkSuccess, DyCkFail);

  Builder.SetInsertPoint(DyCkFail);
  CallInst *TrapCall = Builder.CreateCall(CGM.getIntrinsic(Intrinsic::trap));
  TrapCall->setDoesNotReturn();
  TrapCall->setDoesNotThrow();
  Builder.CreateUnreachable();

  Builder.SetInsertPoint(Begin);
  Builder.CreateCondBr(Cond1, DyCkSuccess, DyCkFallThrough);
  // This ensures the success block comes directly after the branch
  EmitBlock(DyCkSuccess);

  Builder.SetInsertPoint(DyCkSuccess);
}

void CodeGenFunction::EmitDynamicCheckBlocks(Value *Condition) {
  assert(Condition->getType()->isIntegerTy(1) &&
         "May only dynamic check boolean conditions");

  // Constant Folding:
  // If we have generated a constant condition, and the condition is true,
  // then the check will always pass and we can elide it.
  if (const ConstantInt *ConditionConstant = dyn_cast<ConstantInt>(Condition)) {
    if (ConditionConstant->isOne()) {
      ++NumDynamicChecksElided;
      return;
    }
  }

  ++NumDynamicChecksInserted;

  BasicBlock *Begin, *DyCkSuccess, *DyCkFail;
  Begin = Builder.GetInsertBlock();
  DyCkSuccess = createBasicBlock("_Dynamic_check.succeeded");
  DyCkFail = createBasicBlock("_Dynamic_check.failed", this->CurFn);

  Builder.SetInsertPoint(DyCkFail);
  CallInst *TrapCall = Builder.CreateCall(CGM.getIntrinsic(Intrinsic::trap));
  TrapCall->setDoesNotReturn();
  TrapCall->setDoesNotThrow();
  Builder.CreateUnreachable();

  Builder.SetInsertPoint(Begin);
  Builder.CreateCondBr(Condition, DyCkSuccess, DyCkFail);
  // This ensures the success block comes directly after the branch
  EmitBlock(DyCkSuccess);

  Builder.SetInsertPoint(DyCkSuccess);
}

