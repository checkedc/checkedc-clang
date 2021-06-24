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

#include "clang/AST/ExprUtils.h"
#include "clang/Sema/BoundsUtils.h"
#include "TreeTransform.h"

using namespace clang;

bool BoundsUtil::IsStandardForm(const BoundsExpr *BE) {
  BoundsExpr::Kind K = BE->getKind();
  return (K == BoundsExpr::Kind::Any || K == BoundsExpr::Kind::Unknown ||
    K == BoundsExpr::Kind::Range || K == BoundsExpr::Kind::Invalid);
}

BoundsExpr *BoundsUtil::CreateBoundsUnknown(Sema &S) {
  return S.Context.getPrebuiltBoundsUnknown();
}

BoundsExpr *BoundsUtil::ReplaceLValueInBounds(Sema &S, BoundsExpr *Bounds,
                                              Expr *LValue, Expr *OriginalValue,
                                              CheckedScopeSpecifier CSS) {
  Expr *Replaced = ReplaceLValue(S, Bounds, LValue, OriginalValue, CSS);
  if (!Replaced)
    return CreateBoundsUnknown(S);
  else if (BoundsExpr *AdjustedBounds = dyn_cast<BoundsExpr>(Replaced))
    return AdjustedBounds;
  else
    return CreateBoundsUnknown(S);
}

namespace {
  class ReplaceLValueHelper : public TreeTransform<ReplaceLValueHelper> {
    typedef TreeTransform<ReplaceLValueHelper> BaseTransform;
    private:
      Lexicographic Lex;

      // The lvalue expression whose uses should be replaced in an expression.
      Expr *LValue;

      // The original value (if any) to replace uses of the lvalue with.
      // If no original value is provided, an expression using the lvalue
      // will be transformed into an invalid result.
      Expr *OriginalValue;

    public:
      ReplaceLValueHelper(Sema &SemaRef, Expr *LValue, Expr *OriginalValue) :
        BaseTransform(SemaRef),
        Lex(Lexicographic(SemaRef.Context, nullptr)),
        LValue(LValue),
        OriginalValue(OriginalValue) { }

      ExprResult TransformDeclRefExpr(DeclRefExpr *E) {
        DeclRefExpr *V = dyn_cast_or_null<DeclRefExpr>(LValue);
        if (!V)
          return E;
        if (Lex.CompareExpr(V, E) == Lexicographic::Result::Equal) {
          if (OriginalValue)
            return OriginalValue;
          else
            return ExprError();
        } else
          return E;
      }

      ExprResult TransformMemberExpr(MemberExpr *E) {
        MemberExpr *M = dyn_cast_or_null<MemberExpr>(LValue);
        if (!M)
          return E;
        if (Lex.CompareExprSemantically(M, E)) {
          if (OriginalValue)
            return OriginalValue;
          else
            return ExprError();
        } else
          return E;
      }

      // Overriding TransformImplicitCastExpr is necessary since TreeTransform
      // does not preserve implicit casts.
      ExprResult TransformImplicitCastExpr(ImplicitCastExpr *E) {
        // Replace V with OV (if applicable) in the subexpression of E.
        ExprResult ChildResult = TransformExpr(E->getSubExpr());
        if (ChildResult.isInvalid())
          return ChildResult;

        Expr *Child = ChildResult.get();
        CastKind CK = E->getCastKind();

        if (CK == CastKind::CK_LValueToRValue ||
            CK == CastKind::CK_ArrayToPointerDecay)
          // Only cast children of lvalue to rvalue casts to an rvalue if
          // necessary.  The transformed child expression may no longer be
          // an lvalue, depending on the original value.  For example, if x
          // is transformed to the original value x + 1, it does not need to
          // be cast to an rvalue.
          return ExprCreatorUtil::EnsureRValue(SemaRef, Child);
        else
          return ExprCreatorUtil::CreateImplicitCast(SemaRef, Child,
                                                     CK, E->getType());
      }
  };
}

Expr *BoundsUtil::ReplaceLValue(Sema &S, Expr *E, Expr *LValue,
                              Expr *OriginalValue,
                              CheckedScopeSpecifier CSS) {
  // Don't transform E if it does not use the value of LValue.
  if (!ExprUtil::FindLValue(S, LValue, E))
    return E;

  // If E uses the value of LValue, but no original value is provided,
  // we know the result is null without needing to transform E.
  if (!OriginalValue)
    return nullptr;

  // Account for checked scope information when transforming the expression.
  Sema::CheckedScopeRAII CheckedScope(S, CSS);

  Sema::ExprSubstitutionScope Scope(S); // suppress diagnostics
  ExprResult R = ReplaceLValueHelper(S, LValue, OriginalValue).TransformExpr(E);
  if (R.isInvalid())
    return nullptr;
  else
    return R.get();
}
