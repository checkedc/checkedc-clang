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
#include <queue>

namespace clang {
  using DeclSetTy = llvm::DenseSet<const VarDecl *>;
  using BoundsMapTy = llvm::MapVector<const VarDecl *, unsigned>;
  using BlockBoundsTy = llvm::DenseMap<const CFGBlock *, BoundsMapTy>;
  using OrderedBlocksTy = std::vector<const CFGBlock *>;

  class BoundsAnalysis {
  private:
    Sema &S;
    CFG *Cfg;
    ASTContext &Ctx;
    BlockBoundsTy WidenedBounds;

    class ElevatedCFGBlock {
    public:
      const CFGBlock *Block;
      BoundsMapTy In;
      BlockBoundsTy Gen, Out;
      DeclSetTy Kill;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}
    };

    class WorkListTy {
    public:
      std::queue<ElevatedCFGBlock *> Q;
      llvm::DenseSet<ElevatedCFGBlock *> S;

      ElevatedCFGBlock *front() {
        return Q.front();
      }

      void pop(ElevatedCFGBlock *B) {
        Q.pop();
        S.erase(B);
      }

      void push(ElevatedCFGBlock *B) {
        if (!S.count(B)) {
          Q.push(B);
          S.insert(B);
        }
      }

      bool empty() {
        return Q.empty();
      }
    };

    using BlockMapTy = llvm::DenseMap<const CFGBlock *, ElevatedCFGBlock *>;

  public:
    BoundsAnalysis(Sema &S, CFG *Cfg) : S(S), Cfg(Cfg), Ctx(S.Context) {}

    void WidenBounds();
    BoundsMapTy GetWidenedBounds(const CFGBlock *B);
    void DumpWidenedBounds(FunctionDecl *FD);

  private:
    void ComputeGenSets(BlockMapTy BlockMap);
    void ComputeKillSets(BlockMapTy BlockMap);
    void ComputeInSets(ElevatedCFGBlock *EB, BlockMapTy BlockMap);
    void ComputeOutSets(ElevatedCFGBlock *EB, BlockMapTy BlockMap,
                        WorkListTy &Worklist);
    void FillGenSet(Expr *E, ElevatedCFGBlock *EB, const CFGBlock *succ);

    void CollectWidenedBounds(BlockMapTy BlockMap);
    Expr *GetTerminatorCondition(const CFGBlock *B) const;
    bool IsPointerDerefLValue(Expr *E) const;
    bool ContainsPointerDeref(Expr *E) const;
    OrderedBlocksTy GetOrderedBlocks();
    void CollectDefinedVars(const Stmt *S, DeclSetTy &DefinedVars);
    Expr *IgnoreCasts(Expr *E);
    bool SkipBlock(const CFGBlock *B) const;

    template<class T> T Intersect(T &A, T &B);
    template<class T> T Union(T &A, T &B);
    template<class T, class U> T Difference(T &A, U &B);
    template<class T> bool Differ(T &A, T &B);
  };
}

#endif
