//=--CConvInteractiveData.cpp-------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data structure methods
//
//===----------------------------------------------------------------------===//

#include "clang/CConv/CConvInteractiveData.h"

void ConstraintsInfo::Clear() {
  RealWildPtrsWithReasons.clear();
  PtrSourceMap.clear();
  AllWildPtrs.clear();
  TotalNonDirectWildPointers.clear();
  ValidSourceFiles.clear();
  RCMap.clear();
  SrcWMap.clear();
}

CVars &ConstraintsInfo::GetRCVars(ConstraintKey Ckey) {
  return RCMap[Ckey];
}

CVars &ConstraintsInfo::GetSrcCVars(ConstraintKey Ckey) {
  return SrcWMap[Ckey];
}