//===---------- BoundsAnalysis.h - Dataflow for bounds widening-----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//
//
//  This file defines the interface for a dataflow analysis for bounds
//  widening.
//
//===---------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BOUNDS_ANALYSIS_H
#define LLVM_CLANG_BOUNDS_ANALYSIS_H

#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Analysis/CFG.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SetOperations.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallPtrSet.h"

namespace clang {
  using BoundsSet = llvm::SmallPtrSet<const Expr *, 4>;

  class BoundsAnalysis {
  private:
    Sema &S;
    CFG *Cfg;
    ASTContext &Ctx;

    class ElevatedCFGBlock {
    public:
      const CFGBlock *Block;
      BoundsSet In, Out, Gen, Kill;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}
    };

  public:
    BoundsAnalysis(Sema &S, CFG *Cfg) : S(S), Cfg(Cfg), Ctx(S.Context) {}

    void Analyze();

  private:
    std::pair<bool, const Expr *>
      IsBlockValidForAnalysis(ElevatedCFGBlock *B) const;
    VarDecl *getVarDecl(const Expr *E) const;
    bool IsPointerDerefLValue(const Expr *E) const;
    bool ContainsPointerDeref(const Expr *E) const;
  };
}

#endif
