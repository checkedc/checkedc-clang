//=--ABounds.cpp--------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of the methods in ABounds.h.
//
//===----------------------------------------------------------------------===//

#include "clang/CConv/ABounds.h"
#include "clang/CConv/AVarBoundsInfo.h"

ABounds *ABounds::getBoundsInfo(AVarBoundsInfo *ABInfo,
                                BoundsExpr *BExpr,
                                const ASTContext &C) {
  ABounds *Ret = nullptr;
  Expr *NrExpr = BExpr->IgnoreParenImpCasts();
  if (BExpr->isElementCount()) {
    BoundsKey VK = ABInfo->getVariable(NrExpr, C);
    Ret = new CountBound(VK);
  }
  if (BExpr->isByteCount()) {
    BoundsKey VK = ABInfo->getVariable(NrExpr, C);
    Ret = new ByteBound(VK);
  }
  if (BExpr->isRange()) {
    BinaryOperator *BO = dyn_cast<BinaryOperator>(NrExpr);
    assert(BO->getOpcode() == BO_Comma && "Invalid range bounds");
    Expr *LHS = BO->getLHS()->IgnoreParenImpCasts();
    Expr *RHS = BO->getRHS()->IgnoreParenImpCasts();
    BoundsKey LV = ABInfo->getVariable(LHS, C);
    BoundsKey RV = ABInfo->getVariable(RHS, C);
    Ret = new RangeBound(LV, RV);
  }
  return Ret;
}

std::string CountBound::mkString(AVarBoundsInfo *ABI) {
  ProgramVar *PV = ABI->getProgramVar(CountVar);
  assert(PV != nullptr && "No Valid program var");
  return "count(" + PV->mkString() + ")";
}

bool CountBound::areSame(ABounds *O) {
  if (O != nullptr) {
    if (CountBound *OT = dyn_cast<CountBound>(O)) {
      return OT->CountVar == CountVar;
    }
  }
  return false;
}

std::string ByteBound::mkString(AVarBoundsInfo *ABI) {
  ProgramVar *PV = ABI->getProgramVar(ByteVar);
  assert(PV != nullptr && "No Valid program var");
  return "byte_count(" + PV->mkString() + ")";
}

bool ByteBound::areSame(ABounds *O) {
  if (O != nullptr) {
    if (ByteBound *BB = dyn_cast<ByteBound>(O)) {
      return BB->ByteVar == ByteVar;
    }
  }
  return false;
}

std::string RangeBound::mkString(AVarBoundsInfo *ABI) {
  ProgramVar *LBVar = ABI->getProgramVar(LB);
  ProgramVar *UBVar = ABI->getProgramVar(UB);
  assert(LBVar != nullptr && UBVar != nullptr && "No Valid program var");
  return "bounds(" + LBVar->mkString() + ", " + UBVar->mkString() + ")";
}

bool RangeBound::areSame(ABounds *O) {
  if (O != nullptr) {
    if (RangeBound *RB = dyn_cast<RangeBound>(O)) {
      return RB->LB == LB && RB->UB == UB;
    }
  }
  return false;
}
