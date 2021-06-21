//===----------- BoundsUtils.h: Utility functions for bounds ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//
//
//  This file defines the utility functions for bounds expressions.
//
//===------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BOUNDSUTILS_H
#define LLVM_CLANG_BOUNDSUTILS_H

#include "clang/AST/Expr.h"
#include "clang/Sema/Sema.h"

namespace clang {

class BoundsUtil {
public:
  // IsStandardForm returns true if the bounds expression BE is:
  // 1. bounds(any), or:
  // 2. bounds(unknown), or:
  // 3. bounds(e1, e2), or:
  // 4. Invalid bounds.
  // It returns false if BE is of the form count(e) or byte_count(e).
  static bool IsStandardForm(const BoundsExpr *BE);

  // CreateBoundsUnknown returns bounds(unknown).
  static BoundsExpr *CreateBoundsUnknown(Sema &S);
};

} // end namespace clang

#endif
