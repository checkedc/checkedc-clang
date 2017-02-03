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

STATISTIC(DynamicChecksFound, "Number of Dynamic Checks Found");
STATISTIC(DynamicChecksElided, "Number of Dynamic Checks Elided (Due to Constant Folding)");
STATISTIC(DynamicChecksInserted, "Number of Dynamic Checks Found");
STATISTIC(DynamicChecksExplicit, "Number of Dynamic Checks from _Dynamic_check");

void CodeGenFunction::EmitExplicitDynamicCheck(const Expr *Condition) {
  ++DynamicChecksFound;
  ++DynamicChecksExplicit;

  bool ConditionConstant;
  if (ConstantFoldsToSimpleInteger(Condition, ConditionConstant, /*AllowLabels=*/false)
    && ConditionConstant) {
    // Dynamic Check will always pass, leave it out.

    ++DynamicChecksElided;

    return;
  }

  // Dynamic Check relies on runtime behaviour (or we believe it will always fail),
  // so emit the required check
  Value *ConditionVal = EvaluateExprAsBool(Condition);
  EmitDynamicCheckBlocks(ConditionVal);
}

void CodeGenFunction::EmitDynamicCheckBlocks(llvm::Value *Condition) {
  ++DynamicChecksInserted;

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