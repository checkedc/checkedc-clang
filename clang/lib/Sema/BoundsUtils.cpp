//===----------- BoundsUtils.cpp: Utility functions for bounds ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===--------------------------------------------------------------------===//
//
//  This file implements utility functions for bounds expressions.
//
//===--------------------------------------------------------------------===//

#include "clang/Sema/BoundsUtils.h"

using namespace clang;

bool BoundsUtil::IsStandardForm(const BoundsExpr *BE) {
  BoundsExpr::Kind K = BE->getKind();
  return (K == BoundsExpr::Kind::Any || K == BoundsExpr::Kind::Unknown ||
    K == BoundsExpr::Kind::Range || K == BoundsExpr::Kind::Invalid);
}

BoundsExpr *BoundsUtil::CreateBoundsUnknown(Sema &S) {
  return S.Context.getPrebuiltBoundsUnknown();
}
