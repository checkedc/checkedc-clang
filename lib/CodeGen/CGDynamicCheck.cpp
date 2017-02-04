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

STATISTIC(NumDynamicChecksFound,    "The # of dynamic checks found");
STATISTIC(NumDynamicChecksElided,   "The # of dynamic checks elided (due to constant folding)");
STATISTIC(NumDynamicChecksInserted, "The # of dynamic checks found");
STATISTIC(NumDynamicChecksExplicit, "The # of dynamic checks from _Dynamic_check(exp)");

void CodeGenFunction::EmitExplicitDynamicCheck(const Expr *Condition) {
  ++NumDynamicChecksFound;
  ++NumDynamicChecksExplicit;

  bool ConditionConstant;
  if (ConstantFoldsToSimpleInteger(Condition, ConditionConstant, /*AllowLabels=*/false)
    && ConditionConstant) {
    // Dynamic Check will always pass, leave it out.

    ++NumDynamicChecksElided;

    return;
  }

  // Dynamic Check relies on runtime behaviour (or we believe it will always fail),
  // so emit the required check
  Value *ConditionVal = EvaluateExprAsBool(Condition);
  EmitDynamicCheckBlocks(ConditionVal);
}

void CodeGenFunction::EmitDynamicCheckBlocks(llvm::Value *Condition) {
  ++NumDynamicChecksInserted;

  llvm::BasicBlock *Begin, *DyCkSuccess, *DyCkFail;
  Begin = Builder.GetInsertBlock();
  DyCkSuccess = createBasicBlock("_Dynamic_check_succeeded");
  DyCkFail = createBasicBlock("_Dynamic_check_failed", this->CurFn);

  Builder.SetInsertPoint(DyCkFail);
  llvm::CallInst *TrapCall = Builder.CreateCall(CGM.getIntrinsic(llvm::Intrinsic::trap));
  TrapCall->setDoesNotReturn();
  TrapCall->setDoesNotThrow();
  Builder.CreateUnreachable();

  Builder.SetInsertPoint(Begin);
  Builder.CreateCondBr(Condition, DyCkSuccess, DyCkFail);
  // This ensures the success block comes directly after the branch
  EmitBlock(DyCkSuccess);

  Builder.SetInsertPoint(DyCkSuccess);
}