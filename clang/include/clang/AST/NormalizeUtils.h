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

  // Input form:  E1 + (E2 +/- E3)
  // Output form: (E1 + E2) +/- E3
  // Requirements:
  // 1. E1 has pointer type, and:
  // 2. E2 has integer type, and:
  // 3. E3 has integer type.
  static Expr *TransformAssocLeft(Sema &S, Expr *E);

  // GetVariableAndConstant attempts to extract a Variable part and a Constant
  // part from E.
  //
  // If E can be written as C1 +/- C2 +/- ... +/- Cj + E1 +/- Ck +/- ... +/- Cn,
  // where:
  // 1. E1 is a pointer-typed expression, and:
  // 2. Each Ci is an optional integer constant, and:
  // 3. C1 +/- ... +/- Cn does not overflow
  // then:
  // GetVariableAndConstant returns true and sets:
  // 1. Variable = E1
  // 2. Constant = C1 +/- ... +/- Cn
  //
  // GetVariableAndConstant applies left-associativity if possible.
  // If the pointer-typed subexpression E1 of E is of the form p + (i + j),
  // where p is a pointer and i and j are integers, then E1 is rewritten as
  // (p + i) + j. If j is an integer constant, this allows
  // GetVariableAndConstant to add j to the Constant part, and set the
  // Variable part to p + i.
  //
  // Some examples:
  // 1. If E is of the form (p + (i + 1)) + 2:
  //    Variable = p + i, Constant = 3.
  // 2. If E is of the form (p + (1 + i)) + 2:
  //    Variable = (p + 1) + i, Constant = 2.
  // 3. If E is of the form 1 + (2 + (p + (i + 3))):
  //    Variable = p + i, Constant = 6.
  // 4. If E is of the form 1 + (2 + (i + (p + 3))):
  //    Variable = i + (p + 3), Constant = 3.
  static bool GetVariableAndConstant(Sema &S, Expr *E, Expr *&Variable,
                                     llvm::APSInt &Constant);

  // ConstantFold performs simple constant folding operations on E.
  // It attempts to extract a Variable part and a Constant part, based
  // on the form of E.
  //
  // If E is of the form (E1 + A) + B:
  //   Variable = E1, Constant = A + B.
  // If E is of the form (E1 + A) - B:
  //   Variable = E1, Constant = A + -B.
  // If E is of the form (E1 - A) + B:
  //   Variable = E1, Constant = -A + B.
  // If E is of the form (E1 - A) - B:
  //   Variable = E1, Constant = -A + -B.
  //
  // Otherwise, ConstantFold returns false, and:
  //   Variable = E, Constant = 0.
  //
  // TODO: ConstantFold should be replaced with a TransformConstantFold
  // method that returns an expression.
  static bool ConstantFold(Sema &S, Expr *E, QualType T, Expr *&Variable,
                           llvm::APSInt &Constant);

private:
  // Input form:  E1 - E2
  // Output form: E1 + -E2
  // TransformSingleAdditiveOp is a helper method that only performs the
  // transformation on E and not the children of E.
  static Expr *TransformSingleAdditiveOp(Sema &S, Expr *E);

  // AddExprs returns LHS + RHS.
  static Expr *AddExprs(Sema &S, Expr *LHS, Expr *RHS);

  // AddConstants sets C1 to C1 + C2 and returns true if C1 + C2 does not
  // overflow.
  static bool AddConstants(llvm::APSInt &C1, llvm::APSInt C2);

  // If E is of the form E1 +/- C, where C is a constant, GetRHSConstant
  // returns true and sets the out parameter Constant.
  // If E is of the form E1 + C, Constant will be set to C.
  // If E is of the form E1 - C, Constant will be set to -C.
  static bool GetRHSConstant(Sema &S, BinaryOperator *E, QualType T,
                             llvm::APSInt &Constant);

  // If E is of the form P +/- C or C + P, where P is a pointer-typed
  // expression and C is an integer constant, QueryPointerAdditiveConstant
  // returns true and sets:
  // 1. PointerExpr = P
  // 2. Constant = C if E is of the form P + C or C + P
  // 3. Constant = -C if E is of the form P - C
  // Note that E cannot be of the form C - P, since a pointer cannot
  // appear on the right-hand side of a subtraction operator.
  static bool QueryPointerAdditiveConstant(Sema &S, Expr *E,
                                           Expr *&PointerExpr,
                                           llvm::APSInt &Constant);
};

} // end namespace clang
#endif
