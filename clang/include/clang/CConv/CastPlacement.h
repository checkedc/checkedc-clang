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
#include "clang/CConv/ConstraintResolver.h"
#include "clang/CConv/RewriteUtils.h"
#include "clang/CConv/Utils.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CConv/ArrayBoundsInferenceConsumer.h"
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/MappingVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Tooling/Refactoring/SourceCode.h"
#include <sstream>

using namespace clang;

class CastPlacementVisitor :
  public RecursiveASTVisitor<CastPlacementVisitor> { 
    public:
      explicit CastPlacementVisitor(ASTContext *C, ProgramInfo &I, Rewriter &R)
        : Context(C), Info(I), Writer(R), CR(Info, Context)
      {}

      bool VisitCallExpr(CallExpr* C);

    private:

      bool needCasting(ConstraintVariable*, ConstraintVariable*, IsChecked);
      std::string getCastString(ConstraintVariable *Src, 
                                ConstraintVariable *Dst,
                                IsChecked Dinfo) ;
      void surroundByCast(std::string, Expr*);


      ASTContext* Context;
      ProgramInfo& Info;
      Rewriter& Writer;
      ConstraintResolver CR;

};
#endif // _CASTPLACEMENT_H
