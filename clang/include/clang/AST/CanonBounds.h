//===-- CanonBounds.h - compare and canonicalize bounds exprs --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the interface for comparing and canonicalizing
//  bounds expressions.
//
//  Bounds expressions must be non-modifying expressions, so these
//  comparison methods should only be called on non-modifying expressions.
//  In addition, sets of equivalent expressions should also only involve
//  non-modifying expressions.
//
//  TODO: check the invariant about expressions being non-modifying.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_CANON_BOUNDS_H
#define LLVM_CLANG_CANON_BOUNDS_H

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "clang/Basic/AttrKinds.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/OpenMPKinds.h"
#include "clang/Basic/Sanitizers.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>

namespace clang {
  class ASTContext;
  class Expr;
  class VarDecl;

  // List of sets of equivalent expressions.
  typedef SmallVector<SmallVector<Expr *, 4>, 4> EquivExprSets;

  class Lexicographic {
  public:
    enum class Result {
      LessThan,
      Equal,
      GreaterThan
    };

  private:
    ASTContext &Context;
    EquivExprSets *EquivExprs;
    bool Trace;

    template <typename T>
    Lexicographic::Result Compare(const Expr *Raw1, const Expr *Raw2) {
      const T *E1 = dyn_cast<T>(Raw1);
      const T *E2 = dyn_cast<T>(Raw2);
      if (!E1 || !E2) {
        return Result::LessThan;
      }
      return Lexicographic::CompareImpl(E1, E2);
    }

    // See if E1 and E2 are considered equivalent using EquivExprs.  If
    // they are not, return Current.
    Result CheckEquivExprs(Result Current, const Expr *E1, const Expr *E2);

    Result CompareInteger(signed I1, signed I2) const;
    Result CompareInteger(unsigned I1, unsigned I2) const;
    Result CompareRelativeBoundsClause(const RelativeBoundsClause *RC1,
                                       const RelativeBoundsClause *RC2);
    Result CompareScope(const DeclContext *DC1, const DeclContext *DC2) const;

    Result CompareImpl(const PredefinedExpr *E1, const PredefinedExpr *E2);
    Result CompareImpl(const DeclRefExpr *E1, const DeclRefExpr *E2);
    Result CompareImpl(const IntegerLiteral *E1, const IntegerLiteral *E2);
    Result CompareImpl(const FloatingLiteral *E1, const FloatingLiteral *E2);
    Result CompareImpl(const StringLiteral *E1, const StringLiteral *E2);
    Result CompareImpl(const CharacterLiteral *E1, const CharacterLiteral *E2);
    Result CompareImpl(const UnaryOperator *E1, const UnaryOperator *E2);
    Result CompareImpl(const OffsetOfExpr *E1, const OffsetOfExpr *E2);
    Result CompareImpl(const UnaryExprOrTypeTraitExpr *E1,
                   const UnaryExprOrTypeTraitExpr *E2);
    Result CompareImpl(const MemberExpr *E1, const MemberExpr *E2);
    Result CompareImpl(const BinaryOperator *E1, const BinaryOperator *E2);
    Result CompareImpl(const CompoundAssignOperator *E1,
                   const CompoundAssignOperator *E2);
    Result CompareImpl(const CastExpr *E1, const CastExpr *E2);
    Result CompareImpl(const CompoundLiteralExpr *E1,
                   const CompoundLiteralExpr *E2);
    Result CompareImpl(const GenericSelectionExpr *E1,
                   const GenericSelectionExpr *E2);
    Result CompareImpl(const NullaryBoundsExpr *E1,
                       const NullaryBoundsExpr *E2);
    Result CompareImpl(const CountBoundsExpr *E1, const CountBoundsExpr *E2);
    Result CompareImpl(const RangeBoundsExpr *E1, const RangeBoundsExpr *E2);
    Result CompareImpl(const InteropTypeExpr *E1, const InteropTypeExpr *E2);
    Result CompareImpl(const PositionalParameterExpr *E1,
                   const PositionalParameterExpr *E2);
    Result CompareImpl(const BoundsCastExpr *E1, const BoundsCastExpr *E2);
    Result CompareImpl(const CHKCBindTemporaryExpr *E1,
                       const CHKCBindTemporaryExpr *E2);
    Result CompareImpl(const BoundsValueExpr *E1, const BoundsValueExpr *E2);
    Result CompareImpl(const AtomicExpr *E1, const AtomicExpr *E2);
    Result CompareImpl(const BlockExpr *E1, const BlockExpr *E2);


  public:
    Lexicographic(ASTContext &Ctx, EquivExprSets *EquivExprs);

    /// \brief Lexicographic comparison of expressions that can occur in
    /// bounds expressions.
    Result CompareExpr(const Expr *E1, const Expr *E2);
    /// \brief Semantic comparison of expressions that can occur in
    /// bounds expressions. A return value of true indicates that the two
    /// expressions are equivalent semantically.
    bool CompareExprSemantically(const Expr *E1, const Expr *E2);

    /// \brief Compare declarations that may be used by expressions or
    /// or types.
    Result CompareDecl(const NamedDecl *D1, const NamedDecl *D2) const;
    Result CompareType(QualType T1, QualType T2) const;
    Result CompareTypeIgnoreCheckedness(QualType QT1, QualType QT2) const;
    Result CompareTypeLexicographically(QualType QT1, QualType QT2) const;

    Result CompareAPInt(const llvm::APInt &I1, const llvm::APInt &I2) const;
    Expr *IgnoreValuePreservingOperations(ASTContext &Ctx, Expr *E);
  };
}  // end namespace clang

#endif
