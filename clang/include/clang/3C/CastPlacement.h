//=--CastPlacement.h----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains functions and classes that deal with
// placing casts into the text as needing during the rewrite phase
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_CASTPLACEMENT_H
#define LLVM_CLANG_3C_CASTPLACEMENT_H

#include "clang/3C/ConstraintResolver.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "RewriteUtils.h"

// Locates expressions which are children of explicit cast expressions after
// ignoring any intermediate implicit expressions introduced in the clang AST.
class CastLocatorVisitor : public RecursiveASTVisitor<CastLocatorVisitor> {
public:
  explicit CastLocatorVisitor(ASTContext *C) : Context(C) {}

  bool VisitCastExpr(CastExpr *C);

  std::set<Expr *> &getExprsWithCast() { return ExprsWithCast; }

private:
  ASTContext *Context;
  std::set<Expr *> ExprsWithCast;
};

class CastPlacementVisitor : public RecursiveASTVisitor<CastPlacementVisitor> {
public:
  explicit CastPlacementVisitor
    (ASTContext *C, ProgramInfo &I, Rewriter &R, std::set<Expr *> &EWC)
    : Context(C), Info(I), Writer(R), CR(Info, Context), ABRewriter(I),
      ExprsWithCast(EWC) {}

  bool VisitCallExpr(CallExpr *C);

private:
  ASTContext *Context;
  ProgramInfo &Info;
  Rewriter &Writer;
  ConstraintResolver CR;
  ArrayBoundsRewriter ABRewriter;
  std::set<Expr *> &ExprsWithCast;

  // Enumeration indicating what type of cast is required at a call site
  enum CastNeeded {
    NO_CAST = 0,     // No casting required
    CAST_TO_CHECKED, // A CheckedC bounds cast required (wild -> checked)
    CAST_TO_WILD     // A standard C explicit cast required (checked -> wild)
  };

  CastNeeded needCasting(ConstraintVariable *SrcInt,
                         ConstraintVariable *SrcExt,
                         ConstraintVariable *DstInt,
                         ConstraintVariable *DstExt);

  std::pair<std::string, std::string>
  getCastString(ConstraintVariable *Dst, CastNeeded CastKind);

  void surroundByCast(ConstraintVariable *Dst, CastNeeded CastKind, Expr *E);
};
#endif // LLVM_CLANG_3C_CASTPLACEMENT_H
