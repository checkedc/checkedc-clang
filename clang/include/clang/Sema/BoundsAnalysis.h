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
  // For each edge B1->B2, BlockBoundsTy denotes the Gen and Out sets.
  using BlockBoundsTy = llvm::DenseMap<const CFGBlock *, BoundsMapTy>;
  // For each block B, DeclSetTy denotes the Kill set. A VarDecl v is killed if
  // it is assigned to in the block.
  using DeclSetTy = llvm::DenseSet<const VarDecl *>;
  // OrderedBlocksTy denotes blocks ordered by block numbers. This is useful for
  // printing the blocks in a deterministic order.
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
      BoundsMapTy In, WidenedBounds;
      BlockBoundsTy Gen, Out;
      DeclSetTy Kill;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}
    };

    // WorkListContainer is a queue backed by a set. The queue is useful for
    // processing the CFG blocks in a Topological sort order which means that
    // if B1 is a predecessor of B2 then B1 is processed before B2. The set is
    // useful for ensuring only unique blocks are added to the queue.
    template <class T>
    class WorkListContainer {
    private:
      std::queue<T *> Q;
      llvm::DenseSet<T *> S;

    public:
      T *next() const {
        return Q.front();
      }

      void remove(T *B) {
        Q.pop();
        S.erase(B);
      }

      void append(T *B) {
        if (!S.count(B)) {
          Q.push(B);
          S.insert(B);
        }
      }

      bool empty() const {
        return Q.empty();
      }
    };

    // BlockMapTy stores the mapping from CFGBlocks to ElevatedCFGBlocks.
    using BlockMapTy = llvm::DenseMap<const CFGBlock *, ElevatedCFGBlock *>;
    // A list of ElevatedCFGBlocks to run the dataflow analysis on.
    using WorkListTy = WorkListContainer<ElevatedCFGBlock>;

  public:
    BoundsAnalysis(Sema &S, CFG *Cfg) : S(S), Cfg(Cfg), Ctx(S.Context) {}

    // Run the dataflow analysis to widen bounds for _Nt_array_ptr's.
    void WidenBounds();
    // Get the widened bounds for block B.
    BoundsMapTy GetWidenedBounds(const CFGBlock *B);
    // Pretty print the widen bounds analysis.
    void DumpWidenedBounds(FunctionDecl *FD);

  private:
    // Compute Gen set for each edge in the CFG. If there is an edge B1->B2 and
    // the edge condition is of the form "if (*(p + i))" then Gen[B1] = {B2,
    // p:i} . The actual computation of i is done in FillGenSet.
    void ComputeGenSets(BlockMapTy BlockMap);
    // Compute Kill set for each block in BlockMap. For a block B, a variable v
    // is added to Kill[B] if v is assigned to in B.
    void ComputeKillSets(BlockMapTy BlockMap);
    // Compute In set for each block in BlockMap. In[B1] = n Out[B*->B1], where
    // B* are all preds of B1.
    void ComputeInSets(ElevatedCFGBlock *EB, BlockMapTy BlockMap);
    // Compute Out set for each outgoing edge of EB. If the Out set on any edge
    // of EB changes then the successor of EB on that edge is added to
    // Worklist.
    void ComputeOutSets(ElevatedCFGBlock *EB, BlockMapTy BlockMap,
                        WorkListTy &Worklist);
    // Perform checks, handles conditional expressions, extracts the
    // _Nt_array_ptr offset and fills the Gen set for the edge. 
    void FillGenSet(Expr *E, ElevatedCFGBlock *EB, const CFGBlock *succ);
    // For each block, walk over the In set and try to widen bounds.
    void ComputeWidenedBounds(BlockMapTy BlockMap);
    // Assign the widened bounds from the ElevatedBlock to the CFG Block.
    void CollectWidenedBounds(BlockMapTy BlockMap);
    // Get the terminating condition for a block. This could be an if condition
    // of the for "if(*(p + i))".
    Expr *GetTerminatorCondition(const CFGBlock *B) const;
    // Check if E is a pointer dereference.
    bool IsPointerDerefLValue(Expr *E) const;
    // Check if E contains a pointer dereference.
    bool ContainsPointerDeref(Expr *E) const;
    // WidenedBounds is a DenseMap and hence is not suitable for iteration as
    // its iteration order is non-deterministic. So we first need to order the
    // blocks.
    OrderedBlocksTy GetOrderedBlocks();
    // Collect the variables assigned to in a Block.
    void CollectDefinedVars(const Stmt *S, DeclSetTy &DefinedVars);
    // Strip E of all casts.
    Expr *IgnoreCasts(Expr *E);
    // We do not want to run dataflow analysis on null, entry or exit blocks.
    // So we skip them.
    bool SkipBlock(const CFGBlock *B) const;
    // Compute the intersection of sets A and B.
    template<class T> T Intersect(T &A, T &B) const;
    // Compute the union of sets A and B.
    template<class T> T Union(T &A, T &B) const;
    // Compute the set difference of sets A and B.
    template<class T> bool Differ(T &A, T &B) const;
  };
}

#endif
