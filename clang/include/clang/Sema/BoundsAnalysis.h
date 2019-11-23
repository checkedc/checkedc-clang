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
#include "clang/Sema/Sema.h"

namespace clang {
  using BoundsMapTy = llvm::MapVector<const VarDecl *, unsigned>;
  using WidenedBoundsTy = llvm::DenseMap<const CFGBlock *, BoundsMapTy>;
  using OrderedBlocksTy = std::vector<const CFGBlock *>;

  class BoundsAnalysis {
  private:
    Sema &S;
    CFG *Cfg;
    ASTContext &Ctx;
    WidenedBoundsTy WidenedBounds;

    class ElevatedCFGBlock {
    public:
      const CFGBlock *Block;
      BoundsMapTy In, Out, Gen, Kill;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}
    };

    using BlockMapTy = llvm::DenseMap<const CFGBlock *, ElevatedCFGBlock *>;
    using WorkListTy = llvm::SetVector<ElevatedCFGBlock *>;

  public:
    BoundsAnalysis(Sema &S, CFG *Cfg) : S(S), Cfg(Cfg), Ctx(S.Context) {}

    void WidenBounds();
    BoundsMapTy GetWidenedBounds(const CFGBlock *B);
    void DumpWidenedBounds(FunctionDecl *FD);

  private:
    void UpdateGenMap(ElevatedCFGBlock *EB, BlockMapTy BlockMap);
    void UpdateInMap(ElevatedCFGBlock *EB, BlockMapTy BlockMap);
    BoundsMapTy UpdateOutMap(ElevatedCFGBlock *EB);

    void CollectWidenedBounds(BlockMapTy BlockMap);
    const Expr *GetTerminatorCondition(const CFGBlock *B) const;
    const VarDecl *GetVarDecl(const Expr *E) const;
    bool IsPointerDerefLValue(const Expr *E) const;
    bool ContainsPointerDeref(const Expr *E) const;
    OrderedBlocksTy GetOrderedBlocks();

    BoundsMapTy Intersect(BoundsMapTy &A, BoundsMapTy &B);
    BoundsMapTy Union(BoundsMapTy &A, BoundsMapTy &B);
    bool Differ(BoundsMapTy &A, BoundsMapTy &B);
  };
}

#endif
