//===--------- NormalizeUtils.cpp: Functions for normalizing expressions --===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements functions for normalizing expressions.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/NormalizeUtils.h"

using namespace clang;

Expr *NormalizeUtil::AddExprs(Sema &S, Expr *LHS, Expr *RHS) {
  return ExprCreatorUtil::CreateBinaryOperator(S, LHS, RHS,
                            BinaryOperatorKind::BO_Add);
}
