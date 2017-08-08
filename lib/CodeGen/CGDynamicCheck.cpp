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
  STATISTIC(NumDynamicChecksCast, "The # of dynamic cast checks found");
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
  Address Lower = Builder.CreateBitCast(
      EmitPointerWithAlignment(BoundsRange->getLowerExpr()), VoidPtrTy,
      "_Dynamic_check.lower_ptr");
  Address Upper = Builder.CreateBitCast(
      EmitPointerWithAlignment(BoundsRange->getUpperExpr()), VoidPtrTy,
      "_Dynamic_check.upper_ptr");

  // Keep the pointer address
  Address PtrVal =
      Builder.CreateBitCast(PtrAddr, VoidPtrTy, "_Dynamic_check.ptr");

  // Make the lower check
  Value *LowerChk = Builder.CreateICmpULE(
      Lower.getPointer(), PtrVal.getPointer(), "_Dynamic_check.lower");

  // Make the upper check
  Value *UpperChk = Builder.CreateICmpULT(
      PtrVal.getPointer(), Upper.getPointer(), "_Dynamic_check.upper");

  // Emit both checks
  EmitDynamicCheckBlocks(Builder.CreateAnd(LowerChk, UpperChk, "_Dynamic_check.range"));
}

void CodeGenFunction::EmitDynamicBoundsCastCheck(const Address BaseAddr,
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

  ++NumDynamicChecksCast;

  // Emits code as follows:
  // %entry:
  //   ...
  //   %is_null = %base == null
  //   br i1 %is_null, %success, %subsumption
  // %subsumption:
  //   %subsumes = (%lower <= %cast_lower && %cast_upper <= %upper)
  //   br i1 %subsumes, %success, %failure
  // %success:
  //   ...
  // %failure:
  //   trap()

  Address BaseVal =
      Builder.CreateBitCast(BaseAddr, VoidPtrTy, "_Dynamic_check.ptr");
  Value *IsNull =
      Builder.CreateIsNull(BaseVal.getPointer(), "_Dynamic_check.is_null");

  // Constant Folding:
  // If IsNull is true (one), then a) we don't need to insert the rest
  // of the check, computation will continue with success.
  if (const ConstantInt *IsNullConstant = dyn_cast<ConstantInt>(IsNull)) {
    if (IsNullConstant->isOne()) {
      ++NumDynamicChecksElided;
      return;
    }
  }

  BasicBlock *DyCkSubsumption = createBasicBlock("_Dynamic_check.subsumption");
  BasicBlock *DyCkSuccess = createBasicBlock("_Dynamic_check.success");
  BasicBlock *DyCkFail = EmitDynamicCheckFailedBlock();

  // Insert the IsNull Branch
  Builder.CreateCondBr(IsNull, DyCkSuccess, DyCkSubsumption);

  // This ensures the subsumption block comes directly after the is_null branch
  EmitBlock(DyCkSubsumption);

  Builder.SetInsertPoint(DyCkSubsumption);

  // SubRange - bounds(lb, ub) vs CastRange - bounds(castlb, castub)
  // Dynamic_check(lb <= castlb && castub <= ub)

  // Emit the code to generate lb and ub
  Address Lower = Builder.CreateBitCast(
      EmitPointerWithAlignment(SubRange->getLowerExpr()), VoidPtrTy, "");
  Address Upper = Builder.CreateBitCast(
      EmitPointerWithAlignment(SubRange->getUpperExpr()), VoidPtrTy, "");

  // Emit the code to generate castlb and castub
  Address CastLower = Builder.CreateBitCast(
      EmitPointerWithAlignment(CastRange->getLowerExpr()), VoidPtrTy, "");
  Address CastUpper = Builder.CreateBitCast(
      EmitPointerWithAlignment(CastRange->getUpperExpr()), VoidPtrTy, "");

  // Make the lower check (Lower <= CastLower)
  Value *LowerChk = Builder.CreateICmpULE(
      Lower.getPointer(), CastLower.getPointer(), "_Dynamic_check.lower");

  // Make the upper check (CastUpper <= Upper)
  Value *UpperChk = Builder.CreateICmpULE(
      CastUpper.getPointer(), Upper.getPointer(), "_Dynamic_check.upper");

  // Make Both Checks
  Value *CastCond =
      Builder.CreateAnd(LowerChk, UpperChk, "_Dynamic_check.cast");

  // Constant Folding:
  // If IsNull is true (one), then a) we don't need to insert the rest
  // of the check, computation will continue with success.
  if (const ConstantInt *CastCondConstant = dyn_cast<ConstantInt>(CastCond)) {
    if (CastCondConstant->isOne()) {
      ++NumDynamicChecksElided;
      return;
    }
  }

  ++NumDynamicChecksInserted;

  // Insert the CastCond Branch
  Builder.CreateCondBr(CastCond, DyCkSuccess, DyCkFail);

  // This ensures the success block comes directly after the subsumption branch
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

  BasicBlock *DyCkSuccess = createBasicBlock("_Dynamic_check.succeeded");
  BasicBlock *DyCkFail = EmitDynamicCheckFailedBlock();

  Builder.CreateCondBr(Condition, DyCkSuccess, DyCkFail);
  // This ensures the success block comes directly after the branch
  EmitBlock(DyCkSuccess);
  Builder.SetInsertPoint(DyCkSuccess);
}

BasicBlock *CodeGenFunction::EmitDynamicCheckFailedBlock() {
  // Save current insert point
  BasicBlock *Begin = Builder.GetInsertBlock();

  // Add a "failed block", which will be inserted at the end of CurFn
  BasicBlock *DyCkFail = createBasicBlock("_Dynamic_check.failed", CurFn);
  Builder.SetInsertPoint(DyCkFail);
  CallInst *TrapCall = Builder.CreateCall(CGM.getIntrinsic(Intrinsic::trap));
  TrapCall->setDoesNotReturn();
  TrapCall->setDoesNotThrow();
  Builder.CreateUnreachable();

  // Return the insert point back to the saved insert point
  Builder.SetInsertPoint(Begin);

  return DyCkFail;
}
