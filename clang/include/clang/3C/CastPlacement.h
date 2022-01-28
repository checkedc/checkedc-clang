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
#include "clang/3C/RewriteUtils.h"
#include "clang/AST/RecursiveASTVisitor.h"

// Locates expressions which are children of explicit cast expressions after
// ignoring any intermediate implicit expressions introduced in the clang AST.
class CastLocatorVisitor : public RecursiveASTVisitor<CastLocatorVisitor> {
public:
  // This visitor happens to not use the ASTContext itself.
  explicit CastLocatorVisitor(ASTContext *C) {}

  bool VisitCastExpr(CastExpr *C);

  std::set<Expr *> &getExprsWithCast() { return ExprsWithCast; }

private:
  std::set<Expr *> ExprsWithCast;
};

class CastPlacementVisitor : public RecursiveASTVisitor<CastPlacementVisitor> {
public:
  explicit CastPlacementVisitor(ASTContext *C, ProgramInfo &I, Rewriter &R,
                                std::set<Expr *> &EWC)
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

  // Enumeration indicating what type of cast is required at a call site.
  enum CastNeeded {
    NO_CAST = 0,     // No casting required.
    CAST_TO_CHECKED, // A CheckedC bounds cast required (wild -> checked).
    CAST_TO_WILD,    // A standard C explicit cast required (checked -> wild).
    CAST_NT_ARRAY    // A special case cast for assignment to nt_array_ptr from
                     // itype(nt_array_ptr).
  };

  CastNeeded needCasting(ConstraintVariable *SrcInt, ConstraintVariable *SrcExt,
                         ConstraintVariable *DstInt,
                         ConstraintVariable *DstExt);

  std::pair<std::string, std::string> getCastString(ConstraintVariable *Dst,
                                                    ConstraintVariable *TypeVar,
                                                    CastNeeded CastKind);

  void surroundByCast(ConstraintVariable *Dst,
                      ConstraintVariable *TypeVar,
                      CastNeeded CastKind, Expr *E);
  void reportCastInsertionFailure(Expr *E, const std::string &CastStr);
  void updateRewriteStats(CastNeeded CastKind);
};
#endif // LLVM_CLANG_3C_CASTPLACEMENT_H
