//===----------- NormalizeUtils.h: Functions for normalizing expressions --===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the functions for normalizing expressions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_NORMALIZEUTILS_H
#define LLVM_CLANG_NORMALIZEUTILS_H

#include "clang/AST/Expr.h"
#include "clang/Sema/Sema.h"

namespace clang {

class NormalizeUtil {

private:
  // AddExprs returns LHS + RHS.
  static Expr *AddExprs(Sema &S, Expr *LHS, Expr *RHS);
};

} // end namespace clang
#endif
