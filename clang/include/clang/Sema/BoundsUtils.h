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

  // This describes that this is an expression we will never
  // be able to infer bounds for.
  static BoundsExpr *CreateBoundsAlwaysUnknown(Sema &S);

  // If we have an error in our bounds inference that we can't
  // recover from, bounds(unknown) is our error value.
  static BoundsExpr *CreateBoundsInferenceError(Sema &S);

  // Given an array type with constant dimension size, CreateBoundsForArrayType
  // returns a count expression with that size.
  static BoundsExpr *CreateBoundsForArrayType(Sema &S, QualType T,
                                              bool IncludeNullTerminator = false);

  // Compute bounds for a variable expression or member reference expression
  // with an array type.
  static BoundsExpr *ArrayExprBounds(Sema &S, Expr *E,
                                     bool IncludeNullTerminator = false);

  // Given a byte_count or count bounds expression for the VarDecl D,
  // ExpandBoundsToRange will expand it to a range bounds expression.
  //
  // ExpandBoundsToRange creates a DeclRefExpr for D. Clients should not
  // call ExpandBoundsToRange directly. Instead, clients should call
  // Sema::NormalizeBounds, which computes the expanded bounds for D once
  // and attaches the expanded bounds to D.
  static BoundsExpr *ExpandBoundsToRange(Sema &S, VarDecl *D, BoundsExpr *B);

  // Given a byte_count or count bounds expression for the expression Base,
  // expand it to a range bounds expression:
  //  E : Count(C) expands to Bounds(E, E + C)
  //  E : ByteCount(C)  expands to Bounds((array_ptr<char>) E,
  //                                      (array_ptr<char>) E + C)
  static BoundsExpr *ExpandToRange(Sema &S, Expr *Base, BoundsExpr *B);

  // If Bounds uses the value of LValue and an original value is provided,
  // ReplaceLValueInBounds will return a bounds expression where the uses
  // of LValue are replaced with the original value.
  // If Bounds uses the value of LValue and no original value is provided,
  // ReplaceLValueInBounds will return bounds(unknown).
  static BoundsExpr *ReplaceLValueInBounds(Sema &S, BoundsExpr *Bounds,
                                           Expr *LValue, Expr *OriginalValue,
                                           CheckedScopeSpecifier CSS);

  // If an original value is provided, ReplaceLValue returns an expression
  // that replaces all uses of the lvalue expression LValue in E with the
  // original value.  If no original value is provided and E uses LValue,
  // ReplaceLValue returns nullptr.
  static Expr *ReplaceLValue(Sema &S, Expr *E, Expr *LValue,
                             Expr *OriginalValue,
                             CheckedScopeSpecifier CSS);

private:
  // ExpandToRange expands the bounds expression for a variable declaration
  // to a range bounds expression.
  static BoundsExpr *ExpandToRange(Sema &S, VarDecl *D, BoundsExpr *B);
};

} // end namespace clang

#endif
