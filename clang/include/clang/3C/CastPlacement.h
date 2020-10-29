//=--CastPlacement.h-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains functions and classes that deal with
// placing casts into the text as needing during the rewrite phase
//===----------------------------------------------------------------------===//

#ifndef _CASTPLACEMENT_H
#define _CASTPLACEMENT_H
#include "clang/3C/ConstraintResolver.h"
#include "clang/AST/RecursiveASTVisitor.h"

class CastPlacementVisitor : public RecursiveASTVisitor<CastPlacementVisitor> {
public:
  explicit CastPlacementVisitor(ASTContext *C, ProgramInfo &I, Rewriter &R)
      : Context(C), Info(I), Writer(R), CR(Info, Context) {}

  bool VisitCallExpr(CallExpr* C);
private:
  ASTContext* Context;
  ProgramInfo& Info;
  Rewriter& Writer;
  ConstraintResolver CR;

  bool needCasting(ConstraintVariable*, ConstraintVariable*);
  std::string getCastString(ConstraintVariable *Src, ConstraintVariable *Dst);
  void surroundByCast(const std::string&, Expr*);
};
#endif // _CASTPLACEMENT_H