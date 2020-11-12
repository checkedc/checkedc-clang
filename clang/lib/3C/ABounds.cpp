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

#include "clang/3C/ABounds.h"
#include "clang/3C/AVarBoundsInfo.h"

std::set<BoundsKey> ABounds::KeysUsedInBounds;

void ABounds::addBoundsUsedKey(BoundsKey BK) { KeysUsedInBounds.insert(BK); }

bool ABounds::isKeyUsedInBounds(BoundsKey ToCheck) {
  return KeysUsedInBounds.find(ToCheck) != KeysUsedInBounds.end();
}

ABounds *ABounds::getBoundsInfo(AVarBoundsInfo *ABInfo, BoundsExpr *BExpr,
                                const ASTContext &C) {
  ABounds *Ret = nullptr;
  CountBoundsExpr *CBE = dyn_cast<CountBoundsExpr>(BExpr->IgnoreParenCasts());
  RangeBoundsExpr *RBE = dyn_cast<RangeBoundsExpr>(BExpr->IgnoreParenCasts());
  BoundsKey VK;
  if (CBE && !CBE->isCompilerGenerated()) {
    if (ABInfo->tryGetVariable(CBE->getCountExpr()->IgnoreParenCasts(), C,
                               VK)) {
      ProgramVar *PV = ABInfo->getProgramVar(VK);
      if (PV->IsNumConstant() && PV->getVarName() == "0") {
        // Invalid bounds. This is for functions like free.
        // Where the bounds is 0.
        Ret = nullptr;
      } else if (BExpr->isElementCount()) {
        Ret = new CountBound(VK);
      } else if (BExpr->isByteCount()) {
        Ret = new ByteBound(VK);
      }
    }
  }
  if (BExpr->isRange() && RBE) {
    Expr *LHS = RBE->getLowerExpr()->IgnoreParenCasts();
    Expr *RHS = RBE->getUpperExpr()->IgnoreParenImpCasts();
    BoundsKey LV, RV;
    if (ABInfo->tryGetVariable(LHS, C, LV) &&
        ABInfo->tryGetVariable(RHS, C, RV)) {
      Ret = new RangeBound(LV, RV);
    }
  }
  return Ret;
}

std::string CountBound::mkString(AVarBoundsInfo *ABI) {
  ProgramVar *PV = ABI->getProgramVar(CountVar);
  assert(PV != nullptr && "No Valid program var");
  return "count(" + PV->mkString() + ")";
}

bool CountBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (O != nullptr) {
    if (CountBound *OT = dyn_cast<CountBound>(O))
      return ABI->areSameProgramVar(this->CountVar, OT->CountVar);
  }
  return false;
}

BoundsKey CountBound::getBKey() { return this->CountVar; }

ABounds *CountBound::makeCopy(BoundsKey NK) { return new CountBound(NK); }

std::string ByteBound::mkString(AVarBoundsInfo *ABI) {
  ProgramVar *PV = ABI->getProgramVar(ByteVar);
  assert(PV != nullptr && "No Valid program var");
  return "byte_count(" + PV->mkString() + ")";
}

bool ByteBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (O != nullptr) {
    if (ByteBound *BB = dyn_cast<ByteBound>(O)) {
      return ABI->areSameProgramVar(this->ByteVar, BB->ByteVar);
    }
  }
  return false;
}

BoundsKey ByteBound::getBKey() { return this->ByteVar; }

ABounds *ByteBound::makeCopy(BoundsKey NK) { return new ByteBound(NK); }

std::string RangeBound::mkString(AVarBoundsInfo *ABI) {
  ProgramVar *LBVar = ABI->getProgramVar(LB);
  ProgramVar *UBVar = ABI->getProgramVar(UB);
  assert(LBVar != nullptr && UBVar != nullptr && "No Valid program var");
  return "bounds(" + LBVar->mkString() + ", " + UBVar->mkString() + ")";
}

bool RangeBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (O != nullptr) {
    if (RangeBound *RB = dyn_cast<RangeBound>(O)) {
      return ABI->areSameProgramVar(this->LB, RB->LB) &&
             ABI->areSameProgramVar(this->UB, RB->UB);
    }
  }
  return false;
}
