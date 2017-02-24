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

void CodeGenFunction::EmitCheckedCSubscriptCheck(const LValue Addr,
                                                 const BoundsExpr *Bounds) {
  ++NumDynamicChecksFound;
  ++NumDynamicChecksSubscript;

  EmitDynamicBoundsCheck(Addr, Bounds);
}

void CodeGenFunction::EmitCheckedCDerefCheck(const LValue Addr,
                                             const BoundsExpr *Bounds) {
  ++NumDynamicChecksFound;
  ++NumDynamicChecksDeref;

  EmitDynamicBoundsCheck(Addr, Bounds);
}


//
// General Functions for inserting dynamic checks
//

void CodeGenFunction::EmitDynamicNonNullCheck(const LValue Addr) {
  ++NumDynamicChecksNonNull;

  if (!Addr.getType()->isCheckedPointerType())
    return;

  // We can't do constant evaluation here, because we can only be sure a value
  // is null, which would still need the check to be inserted.

  assert(Addr.isSimple() && "Values checked in non-null ptr dynamic checks should be scalars");

  Value *Condition = Builder.CreateIsNotNull(Addr.getPointer(), "_Dynamic_check.non_null");
  EmitDynamicCheckBlocks(Condition);
}

void CodeGenFunction::EmitDynamicBoundsCheck(const LValue Addr, const BoundsExpr *Bounds) {
  if (!Bounds)
    return;

  // We can only generate the check if we have the bounds as a range.
  if (!isa<RangeBoundsExpr>(Bounds)) {
    llvm_unreachable("Can Only Emit Dynamic Bounds Check For RangeBounds Exprs");
    return;
  }

  const RangeBoundsExpr *BoundsRange = dyn_cast<RangeBoundsExpr>(Bounds);

  ++NumDynamicChecksRange;

  // Emit the code to generate the pointer values
  RValue Lower = EmitAnyExpr(BoundsRange->getLowerExpr());
  RValue Upper = EmitAnyExpr(BoundsRange->getUpperExpr());

  assert(Addr.isSimple()
         && Lower.isScalar()
         && Upper.isScalar()
         && "Values checked in range ptr dynamic checks should be scalars");

  // Emit the address as an int
  Value *PtrInt = Builder.CreatePtrToInt(Addr.getPointer(), IntPtrTy, "_Dynamic_check.addr");

  // Make the lower check
  Value *LowerInt = Builder.CreatePtrToInt(Lower.getScalarVal(), IntPtrTy, "_Dynamic_check.lower");
  Value *LowerChk = Builder.CreateICmpSLE(LowerInt, PtrInt, "_Dynamic_check.lower_cmp");

  // Make the upper check
  Value* UpperInt = Builder.CreatePtrToInt(Upper.getScalarVal(), IntPtrTy, "_Dynamic_check.upper");
  Value* UpperChk = Builder.CreateICmpSLT(PtrInt, UpperInt, "_Dynamic_check.upper_cmp");

  // Emit both checks
  EmitDynamicCheckBlocks(Builder.CreateAnd(LowerChk, UpperChk, "_Dynamic_check.range"));
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
