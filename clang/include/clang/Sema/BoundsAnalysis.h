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
  // BoundsMapTy denotes the widened bounds of a variable. Given VarDecl v with
  // declared bounds (low, high), the bounds of v have been widened to (low,
  // high + the unsigned integer).
  using BoundsMapTy = llvm::MapVector<const VarDecl *, unsigned>;
  // For each block B, BlockBoundsTy denotes the Gen and Out sets. For each
  // successor B' of B, BlockBoundsTy stores the bounds for generated on the
  // edge B->B'.
  using BlockBoundsTy = llvm::DenseMap<const CFGBlock *, BoundsMapTy>;
  // For each block B, DeclSetTy denotes the Kill set. A VarDecl v is killed if
  // it is assiged to in the block.
  using DeclSetTy = llvm::DenseSet<const VarDecl *>;
  // OrderedBlocksTy denotes blocks ordered by block numbers. This is useful
  // for deterministic print order.
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

    // WorkListContainer is a queue backed by a set. The queue is useful for
    // processing the CFG blocks in a Topological sort order which means that
    // if A is a predecessor of B then A is processed before B. The set is
    // useful for ensuring only unique blocks are added to the queue.
    template <class T>
    class WorkListContainer {
    public:
      std::queue<T *> Q;
      llvm::DenseSet<T *> S;

      T *front() const {
        return Q.front();
      }

      void pop(T *B) {
        Q.pop();
        S.erase(B);
      }

      void push(T *B) {
        if (!S.count(B)) {
          Q.push(B);
          S.insert(B);
        }
      }

      bool empty() const {
        return Q.empty();
      }
    };

    using BlockMapTy = llvm::DenseMap<const CFGBlock *, ElevatedCFGBlock *>;
    using WorkListTy = WorkListContainer<ElevatedCFGBlock>;

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

    template<class T> T Intersect(T &A, T &B) const;
    template<class T> T Union(T &A, T &B) const;
    template<class T, class U> T Difference(T &A, U &B) const;
    template<class T> bool Differ(T &A, T &B) const;
  };
}

#endif
