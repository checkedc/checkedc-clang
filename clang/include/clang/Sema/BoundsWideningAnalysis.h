//===== BoundsWideningAnalysis.h - Dataflow analysis for bounds widening ====//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
//  This file defines the interface for a dataflow analysis for bounds
//  widening.
//===---------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BOUNDS_WIDENING_ANALYSIS_H
#define LLVM_CLANG_BOUNDS_WIDENING_ANALYSIS_H

#include "clang/AST/CanonBounds.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Sema/CheckedCAnalysesPrepass.h"
#include "clang/Sema/Sema.h"

namespace clang {
  // BoundsMapTy maps a null-terminated array variable to its bounds
  // expression.
  using BoundsMapTy = llvm::DenseMap<const VarDecl *, BoundsExpr *>;

  // StmtBoundsMapTy maps each null-terminated array variable that occurs in a
  // statement to its bounds expression.
  using StmtBoundsMapTy = llvm::DenseMap<const Stmt *, BoundsMapTy>;

  // StmtVarSetTy denotes a set of null-terminated array variables that are
  // associated with a statement. The set of variables whose bounds are killed
  // by a statement has the type StmtVarSetTy.
  using StmtVarSetTy = llvm::DenseMap<const Stmt *, VarSetTy>;

  // The BoundsWideningAnalysis class represents the dataflow analysis for
  // bounds widening. The sets In, Out, Gen and Kill that are used by the
  // analysis are members of this class. The class also has methods that act on
  // these sets to perform the dataflow analysis.
  class BoundsWideningAnalysis {
  private:
    Sema &S;
    CFG *Cfg;
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;

    class ElevatedCFGBlock {
    public:
      const CFGBlock *Block;
      // The In and Out sets for a block.
      BoundsMapTy In, Out;
      // The Gen set for each statement in a block.
      StmtBoundsMapTy Gen;
      // The Kill set for each statement in a block.
      StmtVarSetTy Kill;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}
    };
  
  public:
    BoundsWideningAnalysis(Sema &S, CFG *Cfg) :
      S(S), Cfg(Cfg), Ctx(S.Context),
      Lex(Lexicographic(Ctx, nullptr)),
      OS(llvm::outs()) {}
  };

} // end namespace clang
#endif
