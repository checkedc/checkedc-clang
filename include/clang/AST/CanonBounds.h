//===-- CanonBounds.h - compare and canonicalize bounds exprs --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the interface for comparing and canonicalizing
//  bounds expressions.
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
#include "clang/Basic/VersionTuple.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>

namespace clang {
  class ASTContext;
  class Expr;
  class VarDecl;

  // Abstract base class that provides information what variables
  // currently are equal to each other.
  class EqualityRelation {
  public:
     virtual VarDecl *getRepresentative(VarDecl *V);
  };

  class Lexicographic {
  public:
    enum class Result {
      LessThan,
      Equal,
      GreaterThan
    };

  private:
    EqualityRelation *EqualVars;
    ASTContext &Context;

    Result CompareInteger(signed I1, signed I2);
    Result CompareInteger(unsigned I1, unsigned I2);
    Result ComparePredefinedExpr(const Expr *E1, const Expr *E2);
    Result CompareDeclRefExpr(const Expr *E1, const Expr *E2);
    Result CompareIntegerLiteral(const Expr *E1, const Expr *E2);
    Result CompareFloatingLiteral(const Expr *E1, const Expr *E2);
    Result CompareStringLiteral(const Expr *E1, const Expr *E2);
    Result CompareCharacterLiteral(const Expr *E1, const Expr *E2);
    Result CompareUnaryOperator(const Expr *E1, const Expr *E2);
    Result CompareOffsetOfExpr(const Expr *E1, const Expr *E2);
    Result CompareUnaryExprOrTypeTraitExpr(const Expr *E1, const Expr *E2);
    Result CompareMemberExpr(const Expr *E1, const Expr *E2);
    Result CompareBinaryOperator(const Expr *E1, const Expr *E2);
    Result CompareCompoundAssignOperator(const Expr *E1, const Expr *E2);
    Result CompareImplicitCastExpr(const Expr *E1, const Expr *E2);
    Result CompareCStyleCastExpr(const Expr *E1, const Expr *E2);
    Result CompareCompoundLiteralExpr(const Expr *E1, const Expr *E2);
    Result CompareGenericSelectionExpr(const Expr *E1, const Expr *E2);
    Result CompareNullaryBoundsExpr(const Expr *E1, const Expr *E2);
    Result CompareCountBoundsExpr(const Expr *E1, const Expr *E2);
    Result CompareRangeBoundsExpr(const Expr *E1, const Expr *E2);
    Result CompareInteropTypeBoundsAnnotation(const Expr *E1, const Expr *E2);
    Result ComparePositionalParameterExpr(const Expr *E1, const Expr *E2);
    Result CompareRelativeBoundsClause(const RelativeBoundsClause *RC1,
                                       const RelativeBoundsClause *RC2);
    Result CompareBoundsCastExpr(const Expr *E1, const Expr *E2);
    Result CompareAtomicExpr(const Expr *E1, const Expr *E2);
    Result CompareBlockExpr(const Expr *E1, const Expr *E2);
    Result CompareScope(const DeclContext *DC1, const DeclContext *DC2);

  public:
    Lexicographic(ASTContext &Ctx, EqualityRelation *EV) : 
      Context(Ctx), EqualVars(EV) {
    }

    /// \brief Lexicographic comparison of expressions that can occur in
    /// bounds expressions.
    Result CompareExpr(const Expr *E1, const Expr *E2);

    /// \brief Compare declarations that may be used by expressions or
    /// or types.
    Result CompareDecl(const NamedDecl *D1, const NamedDecl *D2);
    Result CompareType(QualType T1, QualType T2);
  };
}  // end namespace clang

#endif
