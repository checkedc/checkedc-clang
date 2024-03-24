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

BoundsExpr *BoundsUtil::CreateBoundsAlwaysUnknown(Sema &S) {
  return CreateBoundsUnknown(S);
}

BoundsExpr *BoundsUtil::CreateBoundsInferenceError(Sema &S) {
  return CreateBoundsUnknown(S);
}

BoundsExpr *BoundsUtil::CreateBoundsForArrayType(Sema &S, QualType T,
                                                 bool IncludeNullTerminator) {
  const IncompleteArrayType *IAT = S.Context.getAsIncompleteArrayType(T);
  if (IAT) {
    if (IAT->getKind() == CheckedArrayKind::NtChecked)
      return S.Context.getPrebuiltCountZero();
    else
      return CreateBoundsAlwaysUnknown(S);
  }
  const ConstantArrayType *CAT = S.Context.getAsConstantArrayType(T);
  if (!CAT)
    return CreateBoundsAlwaysUnknown(S);

  llvm::APInt size = CAT->getSize();
  // Null-terminated arrays of size n have bounds of count(n - 1).
  // The null terminator is excluded from the count.
  if (!IncludeNullTerminator &&
      CAT->getKind() == CheckedArrayKind::NtChecked) {
    assert(size.uge(1) && "must have at least one element");
    size = size - 1;
  }
  IntegerLiteral *Size =
    ExprCreatorUtil::CreateIntegerLiteral(S.Context, size);
  CountBoundsExpr *CBE =
      new (S.Context) CountBoundsExpr(BoundsExpr::Kind::ElementCount,
                                    Size, SourceLocation(),
                                    SourceLocation());
  return CBE;
}

BoundsExpr *BoundsUtil::ArrayExprBounds(Sema &S, Expr *E,
                                        bool IncludeNullTerminator) {
  DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
  assert((DR && dyn_cast<VarDecl>(DR->getDecl())) || isa<MemberExpr>(E));
  BoundsExpr *BE = CreateBoundsForArrayType(S, E->getType(), IncludeNullTerminator);
  if (BE->isUnknown())
    return BE;

  Expr *Base = ExprCreatorUtil::CreateImplicitCast(S, E,
                                  CastKind::CK_ArrayToPointerDecay,
                                  S.Context.getDecayedType(E->getType()));
  return ExpandToRange(S, Base, BE);
}

BoundsExpr *BoundsUtil::ExpandBoundsToRange(Sema &S, VarDecl *D, BoundsExpr *B) {
  // If the bounds expr is already a RangeBoundsExpr, there is no need
  // to expand it.
  if (B && isa<RangeBoundsExpr>(B))
    return B;

  if (D->getType()->isArrayType()) {
    ExprResult ER = S.BuildDeclRefExpr(D, D->getType(),
                                       clang::ExprValueKind::VK_LValue,
                                       SourceLocation());
    if (ER.isInvalid())
      return nullptr;
    Expr *Base = ER.get();

    // Declared bounds override the bounds based on the array type.
    if (!B)
      return ArrayExprBounds(S, Base);
    Base = ExprCreatorUtil::CreateImplicitCast(S, Base,
                              CastKind::CK_ArrayToPointerDecay,
                              S.Context.getDecayedType(Base->getType()));
    return ExpandToRange(S, Base, B);
  }

  return ExpandToRange(S, D, B);
}

BoundsExpr *BoundsUtil::ExpandToRange(Sema &S, Expr *Base, BoundsExpr *B) {
  assert(Base->isRValue() && "expected rvalue expression");
  if (!B)
    return B;
  BoundsExpr::Kind K = B->getKind();
  switch (K) {
    case BoundsExpr::Kind::ByteCount:
    case BoundsExpr::Kind::ElementCount: {
      CountBoundsExpr *BC = dyn_cast<CountBoundsExpr>(B);
      if (!BC) {
        llvm_unreachable("unexpected cast failure");
        return CreateBoundsInferenceError(S);
      }
      Expr *Count = BC->getCountExpr();
      QualType ResultTy;
      Expr *LowerBound;
      Base = S.MakeAssignmentImplicitCastExplicit(Base);
      if (K == BoundsExpr::ByteCount) {
        ResultTy = S.Context.getPointerType(S.Context.CharTy,
                                            CheckedPointerKind::Array);
        // When bounds are pretty-printed as source code, the cast needs
        // to appear in the source code for the code to be correct, so
        // use an explicit cast operation.
        //
        // The bounds-safe interface argument is false because casts
        // to checked pointer types are always allowed by type checking.
        LowerBound =
          ExprCreatorUtil::CreateExplicitCast(S, ResultTy,
                                              CastKind::CK_BitCast,
                                              Base, false);
      } else {
        ResultTy = Base->getType();
        LowerBound = Base;
        if (ResultTy->isCheckedPointerPtrType()) {
          ResultTy = S.Context.getPointerType(ResultTy->getPointeeType(),
            CheckedPointerKind::Array);
          // The bounds-safe interface argument is false because casts
          // between checked pointer types are always allowed by type
          // checking.
          LowerBound =
            ExprCreatorUtil::CreateExplicitCast(S, ResultTy,
                                                CastKind::CK_BitCast,
                                                Base, false);
        }
      }
      Expr *UpperBound =
        BinaryOperator::Create(S.Context, LowerBound, Count,
                               BinaryOperatorKind::BO_Add,
                               ResultTy,
                               ExprValueKind::VK_RValue,
                               ExprObjectKind::OK_Ordinary,
                               SourceLocation(),
                               FPOptionsOverride());
      RangeBoundsExpr *R = new (S.Context) RangeBoundsExpr(LowerBound, UpperBound,
                                            SourceLocation(),
                                            SourceLocation());
      return R;
    }
    default:
      return B;
  }
}

BoundsExpr *BoundsUtil::ExpandToRange(Sema &S, VarDecl *D, BoundsExpr *B) {
  if (!B)
    return B;
  QualType QT = D->getType();
  ExprResult ER = S.BuildDeclRefExpr(D, QT,
                                     clang::ExprValueKind::VK_LValue, SourceLocation());
  if (ER.isInvalid())
    return nullptr;
  Expr *Base = ER.get();
  if (!QT->isArrayType())
    Base = ExprCreatorUtil::CreateImplicitCast(S, Base, CastKind::CK_LValueToRValue, QT);
  return ExpandToRange(S, Base, B);
}

BoundsExpr *BoundsUtil::ReplaceLValueInBounds(Sema &S, BoundsExpr *Bounds,
                                              Expr *LValue, Expr *OriginalValue,
                                              CheckedScopeSpecifier CSS) {
  if (Bounds->isUnknown() || Bounds->isAny())
    return Bounds;
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
          return ExprError();
        }
        return E;
      }

      ExprResult TransformMemberExpr(MemberExpr *E) {
        MemberExpr *M = dyn_cast_or_null<MemberExpr>(LValue);
        if (!M)
          return E;
        if (Lex.CompareExprSemantically(M, E)) {
          if (OriginalValue)
            return OriginalValue;
          return ExprError();
        }
        return E;
      }

      ExprResult TransformUnaryOperator(UnaryOperator *E) {
        if (!ExprUtil::IsDereferenceOrSubscript(LValue))
          return E;
        if (Lex.CompareExprSemantically(LValue, E)) {
          if (OriginalValue)
            return OriginalValue;
          return ExprError();
        }
        return E;
      }

      ExprResult TransformArraySubscriptExpr(ArraySubscriptExpr *E) {
        if (!ExprUtil::IsDereferenceOrSubscript(LValue))
          return E;
        if (Lex.CompareExprSemantically(LValue, E)) {
          if (OriginalValue)
            return OriginalValue;
          return ExprError();
        }
        return E;
      }

      // Overriding TransformImplicitCastExpr is necessary since TreeTransform
      // does not preserve implicit casts.
      ExprResult TransformImplicitCastExpr(ImplicitCastExpr *E) {
        // Replace LValue with OriginalValue (if applicable) in the
        // subexpression of E.
        ExprResult ChildResult = TransformExpr(E->getSubExpr());
        if (ChildResult.isInvalid())
          return ChildResult;

        Expr *Child = ChildResult.get();
        CastKind CK = E->getCastKind();

        // Only cast children of lvalue to rvalue or array to pointer casts
        // to an rvalue if necessary. The transformed child expression may
        // no longer be an lvalue, depending on the original value.
        // For example, if x is transformed to the original value x + 1, it
        // does not need to be cast to an rvalue.
        if (CK == CastKind::CK_LValueToRValue ||
            CK == CastKind::CK_ArrayToPointerDecay)
          return ExprCreatorUtil::EnsureRValue(SemaRef, Child);

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
