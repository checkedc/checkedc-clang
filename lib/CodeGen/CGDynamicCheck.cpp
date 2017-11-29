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

void CodeGenFunction::EmitDynamicBoundsCheck(const Address PtrAddr, const BoundsExpr *Bounds,
                                             BoundsCheckKind CheckKind, llvm::Value *Val) {
  if (!getLangOpts().CheckedC)
    return;

  if (!Bounds)
    return;

  if (Bounds->isAny() || Bounds->isInvalid())
    return;

  // We'll insert the bounds check for an assignment through a null-terminated pointer
  // later, when we know the value.
  if (CheckKind == BoundsCheckKind::BCK_NullTermWriteAssign && !Val)
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

  // We don't infer an expression with the correct cast for
  // multidimensional array access, but icmp requires that
  // its operands are of the same type, so we bitcast Lower to
  // match the type of PtrAddr at the LLVM IR Level.
  if (Lower.getType() != PtrAddr.getType())
    Lower = Builder.CreateBitCast(Lower, PtrAddr.getType());

  Address Upper = EmitPointerWithAlignment(BoundsRange->getUpperExpr());

  // As above, we may need to bitcast Upper to match the type
  // of PtrAddr at the LLVM IR Level.
  if (Upper.getType() != PtrAddr.getType())
    Upper = Builder.CreateBitCast(Upper, PtrAddr.getType());

  // Make the lower check
  Value *LowerChk = Builder.CreateICmpULE(
      Lower.getPointer(), PtrAddr.getPointer(), "_Dynamic_check.lower");

  // Make the upper check
  Value *UpperChk;
  assert(CheckKind != BCK_None);
  if (CheckKind != BCK_NullTermRead)
    UpperChk = Builder.CreateICmpULT(PtrAddr.getPointer(), Upper.getPointer(),
                                     "_Dynamic_check.upper");
  else
    // For reads of null-terminated pointers, we allow the element exactly
    // at the upper bound to be read.
    UpperChk = Builder.CreateICmpULE(PtrAddr.getPointer(), Upper.getPointer(),
                                     "_Dynamic_check.upper");
  llvm::Value *Condition = Builder.CreateAnd(LowerChk, UpperChk, "_Dynamic_check.range");
  if (const ConstantInt *ConditionConstant = dyn_cast<ConstantInt>(Condition)) {
    if (ConditionConstant->isOne())
      return;
  }
  BasicBlock *DyCkSuccess = createBasicBlock("_Dynamic_check.succeeded");
  BasicBlock *DyCkFailure;
  if (CheckKind == BCK_NullTermWriteAssign)
    DyCkFailure = EmitNulltermWriteAdditionalCheck(PtrAddr, Upper, LowerChk,
                                                   Val, DyCkSuccess);
  else
    DyCkFailure = EmitDynamicCheckFailedBlock();
  Builder.CreateCondBr(Condition, DyCkSuccess, DyCkFailure);
  // This ensures the success block comes directly after the branch
  EmitBlock(DyCkSuccess);
  Builder.SetInsertPoint(DyCkSuccess);
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
  //
  // %entry:
  //   ... (prior code)
  //   %is_null = %base == null
  //   br i1 %is_null, %success, %subsumption
  // %subsumption:
  //   %subsumes = (%lower <= %cast_lower && %cast_upper <= %upper)
  //   br i1 %subsumes, %success, %failure
  // %success:
  //   ... (following code)
  // %failure:
  //   trap()

  Value *IsNull =
      Builder.CreateIsNull(BaseAddr.getPointer(), "_Dynamic_check.is_null");

  // Constant Folding:
  // If IsNull is true (one), then we don't need to insert the rest
  // of the check, as computation should continue without inserting
  // the branch.
  if (const ConstantInt *IsNullConstant = dyn_cast<ConstantInt>(IsNull)) {
    if (IsNullConstant->isOne()) {
      ++NumDynamicChecksElided;

      // We have not emitted any blocks or any branches so far,
      // so we can just return here, which leaves the Builder
      // in the right position to add instructions to the end of
      // the entry block.
      //
      // The code will look like:
      // %entry:
      //   ... (prior code)
      //   %is_null = true
      //   ... (following code)
      // (No %failure Block)

      return;
    }
  }

  BasicBlock *DyCkSubsumption = createBasicBlock("_Dynamic_check.subsumption");
  BasicBlock *DyCkSuccess = createBasicBlock("_Dynamic_check.success");

  // Insert the IsNull Branch
  Builder.CreateCondBr(IsNull, DyCkSuccess, DyCkSubsumption);

  // This ensures the subsumption block comes directly after the IsNull branch
  EmitBlock(DyCkSubsumption);

  Builder.SetInsertPoint(DyCkSubsumption);

  // SubRange - bounds(lb, ub) vs CastRange - bounds(castlb, castub)
  // Dynamic_check(lb <= castlb && castub <= ub)
  // If required, we will be bitcasting castlb and castub at the
  // LLVM IR level to match the types of lb and ub respectively.

  // Emit the code to generate pointers for SubRange, lb and ub
  Address Lower = EmitPointerWithAlignment(SubRange->getLowerExpr());
  Address Upper = EmitPointerWithAlignment(SubRange->getUpperExpr());

  // Emit the code to generate pointers for CastRange, castlb and castub

  Address CastLower = EmitPointerWithAlignment(CastRange->getLowerExpr());
  // We will be comparing CastLower to Lower. Their types may not match,
  // so we're going to bitcast CastLower to match the type of Lower if needed.
  if (CastLower.getType() != Lower.getType())
    CastLower = Builder.CreateBitCast(CastLower, Lower.getType());

  Address CastUpper = EmitPointerWithAlignment(CastRange->getUpperExpr());
  // Again we're going to bitcast CastUpper to match the type of Upper
  // if needed.
  if (CastUpper.getType() != Upper.getType())
    CastUpper = Builder.CreateBitCast(CastUpper, Upper.getType());

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
  // If CastCond is true (one), then we need to insert a direct branch
  // to the success block, emit it, and set the insert point there for
  // further code generation.
  if (const ConstantInt *CastCondConstant = dyn_cast<ConstantInt>(CastCond)) {
    if (CastCondConstant->isOne()) {
      ++NumDynamicChecksElided;

      // We have emitted a branch to the failure block, along with the
      // failure block, so we have to emit a direct branch to success,
      //
      // The code will look like this:
      // %entry:
      //   ... (prior code)
      //   %is_null = %base == null
      //   br i1 %is_null, %success, %subsumption
      // %subsumption:
      //   %subsumes = true
      //   br %success
      // %success:
      //   ... (following code)
      // (No %failure Block)

      // This check will always pass, directly jump to the success block.
      Builder.CreateBr(DyCkSuccess);

      // This ensures the success block comes directly after the subsumption branch
      EmitBlock(DyCkSuccess);
      Builder.SetInsertPoint(DyCkSuccess);

      return;
    }
  }

  ++NumDynamicChecksInserted;

  BasicBlock *DyCkFail = EmitDynamicCheckFailedBlock();

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
  BasicBlock *FailBlock = createBasicBlock("_Dynamic_check.failed", CurFn);
  Builder.SetInsertPoint(FailBlock);
  CallInst *TrapCall = Builder.CreateCall(CGM.getIntrinsic(Intrinsic::trap));
  TrapCall->setDoesNotReturn();
  TrapCall->setDoesNotThrow();
  Builder.CreateUnreachable();

  // Return the insert point back to the saved insert point
  Builder.SetInsertPoint(Begin);

  return FailBlock;
}

BasicBlock *CodeGenFunction::EmitNulltermWriteAdditionalCheck(
   const Address PtrAddr,
   const Address Upper,
   llvm::Value *LowerChk,
   llvm::Value *Val,
   BasicBlock *Succeeded) {
  // Save current insert point
  BasicBlock *Begin = Builder.GetInsertBlock();

  // Add a "failed block", which will be inserted at the end of CurFn
  BasicBlock *FailBlock = createBasicBlock("_Nullterm_range_check.failed", CurFn);
  Builder.SetInsertPoint(FailBlock);
  Value *AtUpper = Builder.CreateICmpEQ(PtrAddr.getPointer(), Upper.getPointer(),
                                        "_Dynamic_check.at_upper");
  BasicBlock *OnFailure = EmitDynamicCheckFailedBlock();
  llvm::Value *Condition1 = Builder.CreateAnd(LowerChk, AtUpper, "_Dynamic_check.nt_upper_bound");
  Value *IsZero = Builder.CreateIsNull(Val, "_Dynamic_check.write_nul");
  llvm::Value *Condition2 = Builder.CreateAnd(Condition1, IsZero, "_Dynamic_check.allowed_write");
  Builder.CreateCondBr(Condition2, Succeeded, OnFailure);
  // Return the insert point back to the saved insert point
  Builder.SetInsertPoint(Begin);

  return FailBlock;
}

BoundsExpr *CodeGenFunction::GetNullTermBoundsCheck(Expr *E) {
  E = E->IgnoreParenCasts();
  switch (E->getStmtClass()) {
    case Expr::UnaryOperatorClass: {
      UnaryOperator *UO = cast<UnaryOperator>(E);
      if (UO->getBoundsCheckKind() == BoundsCheckKind::BCK_NullTermWriteAssign)
        return UO->getBoundsExpr();
      break;
    }
    case Expr::ArraySubscriptExprClass: {
      ArraySubscriptExpr *AS = cast<ArraySubscriptExpr>(E);
      if (AS->getBoundsCheckKind() == BoundsCheckKind::BCK_NullTermWriteAssign)
        return AS->getBoundsExpr();
      break;
    }
    default:
      break;
  }
  return nullptr;
}