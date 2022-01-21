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

ABounds *ABounds::getBoundsInfo(AVarBoundsInfo *ABInfo, BoundsExpr *BExpr,
                                const ASTContext &C) {
  ABounds *Ret = nullptr;
  CountBoundsExpr *CBE = dyn_cast<CountBoundsExpr>(BExpr->IgnoreParenCasts());
  BoundsKey VK;
  if (CBE && !CBE->isCompilerGenerated()) {
    if (ABInfo->tryGetVariable(CBE->getCountExpr()->IgnoreParenCasts(), C,
                               VK)) {
      ProgramVar *PV = ABInfo->getProgramVar(VK);
      if (PV->isNumConstant() && PV->getConstantVal() == 0) {
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
  return Ret;
}

std::string ABounds::getBoundsKeyStr(BoundsKey BK, AVarBoundsInfo *ABI,
                                     Decl *D) {
  ProgramVar *PV = ABI->getProgramVar(BK);
  assert(PV != nullptr && "No Valid program var");
  std::string BKStr = PV->getVarName();
  unsigned PIdx = 0;
  auto *PVD = dyn_cast_or_null<ParmVarDecl>(D);
  // Does this belong to a function parameter?
  if (PVD && ABI->isFuncParamBoundsKey(BK, PIdx)) {
    // Then get the corresponding parameter in context of the given
    // Function and get its name.
    const FunctionDecl *FD = dyn_cast<FunctionDecl>(PVD->getDeclContext());
    if (FD->getNumParams() > PIdx) {
      auto *NewPVD = FD->getParamDecl(PIdx);
      BKStr = NewPVD->getNameAsString();
      // If the parameter in the new declaration does not have a name?
      // then use the old name.
      if (BKStr.empty())
        BKStr = PV->getVarName();
    }
  }
  return BKStr;
}

std::string
ABounds::mkString(AVarBoundsInfo *ABI, clang::Decl *D, BoundsKey BK) {
  if (LowerBoundKey != 0 && LowerBoundKey != BK)
    return mkStringWithLowerBound(ABI, D);
  return mkStringWithoutLowerBound(ABI, D);
}

std::string
CountBound::mkStringWithLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) {
  std::string LowerBound = ABounds::getBoundsKeyStr(LowerBoundKey, ABI, D);
  // Assume that LowerBound is the same pointer type as this pointer this bound
  // acts on, so pointer arithmetic works as expected.
  return "bounds(" + LowerBound + ", " + LowerBound + " + " +
         ABounds::getBoundsKeyStr(LengthKey, ABI, D) + ")";
}

std::string
CountBound::mkStringWithoutLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) {
  return "count(" + ABounds::getBoundsKeyStr(LengthKey, ABI, D) + ")";
}

bool CountBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (O != nullptr) {
    if (CountBound *OT = dyn_cast<CountBound>(O))
      return ABI->areSameProgramVar(this->LengthKey, OT->LengthKey);
  }
  return false;
}

ABounds *CountBound::makeCopy(BoundsKey NK) { return new CountBound(NK); }

std::string CountPlusOneBound::mkStringWithLowerBound(AVarBoundsInfo *ABI,
                                                      clang::Decl *D) {

  std::string LowerBound = ABounds::getBoundsKeyStr(LowerBoundKey, ABI, D);
  return "bounds(" + LowerBound + ", " + LowerBound + " + " +
         ABounds::getBoundsKeyStr(LengthKey, ABI, D) + " + 1)";
}

std::string CountPlusOneBound::mkStringWithoutLowerBound(AVarBoundsInfo *ABI,
                                                         clang::Decl *D) {
  return "count(" + ABounds::getBoundsKeyStr(LengthKey, ABI, D) + " + 1)";
}

bool CountPlusOneBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (CountPlusOneBound *OT = dyn_cast_or_null<CountPlusOneBound>(O))
    return ABI->areSameProgramVar(this->LengthKey, OT->LengthKey);
  return false;
}

std::string ByteBound::mkStringWithLowerBound(AVarBoundsInfo *ABI,
                                              clang::Decl *D) {
  std::string LowerBound = ABounds::getBoundsKeyStr(LowerBoundKey, ABI, D);
  // LowerBound will be a pointer to some type that is not necessarily char, so
  // pointer arithmetic will not give the behavior needed for a byte count.
  // First cast the pointer to a char pointer, and then add byte count.
  return "bounds(((_Array_ptr<char>)" + LowerBound + "), ((_Array_ptr<char>)" +
         LowerBound + ") + " + ABounds::getBoundsKeyStr(LengthKey, ABI, D) + ")";
}

std::string ByteBound::mkStringWithoutLowerBound(AVarBoundsInfo *ABI,
                                                 clang::Decl *D) {
  return "byte_count(" + ABounds::getBoundsKeyStr(LengthKey, ABI, D) + ")";
}

bool ByteBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (ByteBound *BB = dyn_cast_or_null<ByteBound>(O))
    return ABI->areSameProgramVar(this->LengthKey, BB->LengthKey);
  return false;
}

ABounds *ByteBound::makeCopy(BoundsKey NK) { return new ByteBound(NK); }
