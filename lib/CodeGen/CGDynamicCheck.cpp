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

STATISTIC(NumDynamicChecksFound,    "The # of dynamic checks found (total)");
STATISTIC(NumDynamicChecksElided,   "The # of dynamic checks elided (due to constant folding)");
STATISTIC(NumDynamicChecksInserted, "The # of dynamic checks inserted");

STATISTIC(NumDynamicChecksNonNull, "The # of dynamic non-null checks found");
STATISTIC(NumDynamicChecksRange,   "The # of dynamic bounds checks found");

STATISTIC(NumDynamicChecksExplicit,  "The # of dynamic checks inserted from _Dynamic_check(exp)");
STATISTIC(NumDynamicChecksSubscript, "The # of dynamic checks inserted from exp[exp]");
STATISTIC(NumDynamicChecksMember,    "The # of dynamic checks inserted from exp.f");
STATISTIC(NumDynamicChecksDeref,     "The # of dynamic checks inserted from *exp");
STATISTIC(NumDynamicChecksLValRead,  "The # of dynamic checks inserted from l-value reads");
STATISTIC(NumDynamicChecksAssign,    "The # of dynamic checks inserted from x = exp, x += exp");
STATISTIC(NumDynamicChecksIncrement, "The # of dynamic checks inserted from x++, x--, ++x, --x");

//
// Expression-specific dynamic check insertion
//

void CodeGenFunction::EmitExplicitDynamicCheck(const Expr *Condition) {
  ++NumDynamicChecksFound;

  bool ConditionConstant;
  if (ConstantFoldsToSimpleInteger(Condition, ConditionConstant, /*AllowLabels=*/false)
      && ConditionConstant) {

    // Dynamic Check will always pass, leave it out.
    ++NumDynamicChecksElided;

    return;
  }

  ++NumDynamicChecksExplicit;

  // Dynamic Check relies on runtime behaviour (or we believe it will always fail),
  // so emit the required check
  Value *ConditionVal = EvaluateExprAsBool(Condition);
  EmitDynamicCheckBlocks(ConditionVal);
}

void CodeGenFunction::EmitCheckedCSubscriptCheck(const Expr *E,
                                                 const Expr *Base,
                                                 Value *Idx, QualType IdxTy) {
  ++NumDynamicChecksFound;
  ++NumDynamicChecksSubscript;
}


//
// General Functions for inserting dynamic checks
//

void CodeGenFunction::EmitDynamicNonNullCheck(const Expr *CheckedPtr) {
  ++NumDynamicChecksNonNull;

  if (!CheckedPtr->getType()->isCheckedPointerType())
    return;

  // We can't do constant evaluation here, because we can only be sure a value
  // is null, which would still need the check to be inserted.

  RValue CheckedPtrVal = EmitAnyExprToTemp(CheckedPtr);
  assert(CheckedPtrVal.isScalar() && "Values checked in non-null ptr dynamic checks should be scalars");

  Value *Condition = Builder.CreateIsNotNull(CheckedPtrVal.getScalarVal(), "_Dynamic_check.non_null");
  EmitDynamicCheckBlocks(Condition);
}

void CodeGenFunction::EmitDynamicBoundsCheck(const Expr *CheckedPtr, const BoundsExpr *Bounds) {
  ++NumDynamicChecksRange;

  // If the bounds are invalid, we don't do the check.
  if (Bounds->isInvalid())
    return;

  // We can only generate the check if we have the bounds as a range.
  if (!isa<RangeBoundsExpr>(Bounds))
    return;
  const RangeBoundsExpr *BoundsRange = dyn_cast<RangeBoundsExpr>(Bounds);

  // Constant Evaluation
  bool LowerChkNeeded = true, UpperChkNeeded = true;
  APSInt PtrConstant, LowerConstant, UpperConstant;
  // We need Ptr to evaluate to a constant if we want to leave out any of these checks
  if (ConstantFoldsToSimpleInteger(CheckedPtr, PtrConstant, /*AllowLabels=*/false)) {
    // If we can find the lower bound, and it is lower than the ptr, then we don't need to do the lower bounds check
    if (ConstantFoldsToSimpleInteger(BoundsRange->getLowerExpr(), LowerConstant, /*AllowLabels=*/false)) {
      LowerChkNeeded = !(LowerConstant <= PtrConstant);
    }

    // If we can find the upper bound, and it is higher than the ptr, then we don't need to do the upper bounds check
    if (ConstantFoldsToSimpleInteger(BoundsRange->getUpperExpr(), UpperConstant, /*AllowLabels=*/false)) {
      UpperChkNeeded = !(PtrConstant < UpperConstant);
    }

    // Neither check is needed, leave both out
    if (!LowerChkNeeded && !UpperChkNeeded) {
      ++NumDynamicChecksElided;
      return;
    }
  }
  // End Constant Evaluation

  // Emit the code to generate the pointer values
  RValue PtrVal, Lower, Upper;
  PtrVal = EmitAnyExpr(CheckedPtr);
  if (LowerChkNeeded)
    Lower = EmitAnyExpr(BoundsRange->getLowerExpr());
  if (UpperChkNeeded)
    Upper = EmitAnyExpr(BoundsRange->getUpperExpr());

  assert(PtrVal.isScalar()
         && (!LowerChkNeeded || Lower.isScalar())
         && (!UpperChkNeeded || Upper.isScalar())
         && "Values checked in range ptr dynamic checks should be scalars");

  Value *PtrInt = Builder.CreateIntCast(PtrVal.getScalarVal(), IntPtrTy, /*isSigned=*/false);

  llvm::Value *LowerChk, *UpperChk;

  // Make the lower check
  if (LowerChkNeeded) {
    Value *LowerInt = Builder.CreateIntCast(Lower.getScalarVal(), IntPtrTy, /*isSigned=*/false);
    LowerChk = Builder.CreateICmpULE(LowerInt, PtrInt, "_Dynamic_check.lower");
  }

  // Make the upper check
  if (UpperChkNeeded) {
    Value* UpperInt = Builder.CreateIntCast(Upper.getScalarVal(), IntPtrTy, /*isSigned=*/false);
    UpperChk = Builder.CreateICmpULT(PtrInt, UpperInt, "_Dynamic_check.upper");
  }

  if (UpperChkNeeded && LowerChkNeeded) {
    // Insert both checks
    EmitDynamicCheckBlocks(Builder.CreateAnd(LowerChk, UpperChk, "_Dynamic_check.range"));
  }
  else if (LowerChkNeeded && !UpperChkNeeded) {
    // Emit only the lower check
    EmitDynamicCheckBlocks(LowerChk);
  }
  else if (!LowerChkNeeded && UpperChkNeeded) {
    // Emit only the upper check
    EmitDynamicCheckBlocks(UpperChk);
  }
}

void CodeGenFunction::EmitDynamicCheckBlocks(Value *Condition) {
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
