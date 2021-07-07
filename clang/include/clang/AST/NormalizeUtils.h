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
public:
  // Each Transform* method returns an expression of the form
  // <Output form>, if the input expression E is of the form
  // <Input form> and E meets the specified requirements.
  // Otherwise, the method returns nullptr.
  // Each Transform* method looks at, at most:
  // 1. E
  // 2. The children of E
  // 3. The grandchildren of E

  // Input form: E1 - E2
  // Output form: E1 + -E2
  // This transformation will also be applied to E1 and E2.
  static Expr *TransformAdditiveOp(Sema &S, Expr *E);

private:
  // Input form:  E1 - E2
  // Output form: E1 + -E2
  // TransformSingleAdditiveOp is a helper method that only performs the
  // transformation on E and not the children of E.
  static Expr *TransformSingleAdditiveOp(Sema &S, Expr *E);

  // AddExprs returns LHS + RHS.
  static Expr *AddExprs(Sema &S, Expr *LHS, Expr *RHS);

  // If E is of the form E1 + E2, GetAdditionOperands returns true
  // and sets LHS to E1 and RHS to E2.
  static bool GetAdditionOperands(Expr *E, Expr *&LHS, Expr *&RHS);
};

} // end namespace clang
#endif
