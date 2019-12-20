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

#include "clang/AST/CanonBounds.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Sema/Sema.h"
#include <queue>

namespace clang {
  // QueueSet is a queue backed by a set. The queue is useful for processing
  // the items in a Topological sort order which means that if item1 is a
  // predecessor of item2 then item1 is processed before item2. The set is
  // useful for maintaining uniqueness of items added to the queue.

  template <class T>
  class QueueSet {
  private:
    std::queue<T *> _queue;
    llvm::DenseSet<T *> _set;

  public:
    T *next() const {
      return _queue.front();
    }

    void remove(T *B) {
      if (_queue.empty())
        return;
      _queue.pop();
      _set.erase(B);
    }

    void append(T *B) {
      if (!_set.count(B)) {
        _queue.push(B);
        _set.insert(B);
      }
    }

    bool empty() const {
      return _queue.empty();
    }
  };

} // end namespace clang

namespace clang {
  // Note: We use the shorthand "ntptr" to denote _Nt_array_ptr. We extract the
  // declaration of an ntptr as a VarDecl from a DeclRefExpr.

  // BoundsMapTy denotes the widened bounds of an ntptr. Given VarDecl V with
  // declared bounds (low, high), the bounds of V have been widened to (low,
  // high + the unsigned integer).
  using BoundsMapTy = llvm::MapVector<const VarDecl *, unsigned>;

  // For each edge B1->B2, EdgeBoundsTy denotes the Gen and Out sets.
  using EdgeBoundsTy = llvm::DenseMap<const CFGBlock *, BoundsMapTy>;

  // For each block B, DeclSetTy denotes the Kill set. A VarDecl V is killed if:
  // 1. V is assigned to in the block, or
  // 2. any variable used in the bounds expr of V is assigned to in the block.
  using DeclSetTy = llvm::DenseSet<const VarDecl *>;

  // A mapping of VarDecl V to all the variables occuring in its bounds
  // expression. This is used to compute Kill sets. An assignment to any
  // variable occuring in the bounds expression of an ntptr kills any computed
  // bounds for that ntptr in that block.
  using BoundsVarTy = llvm::DenseMap<const VarDecl *, DeclSetTy>;

  // OrderedBlocksTy denotes blocks ordered by block numbers. This is useful
  // for printing the blocks in a deterministic order.
  using OrderedBlocksTy = std::vector<const CFGBlock *>;

  class BoundsAnalysis {
  private:
    Sema &S;
    CFG *Cfg;
    ASTContext &Ctx;
    // The final widened bounds will reside here. This is a map keyed by
    // CFGBlock.
    EdgeBoundsTy WidenedBounds;

    class ElevatedCFGBlock {
    public:
      const CFGBlock *Block;
      // The In set for the block.
      BoundsMapTy In;
      // The Gen and Out sets for the block.
      EdgeBoundsTy Gen, Out;
      // The Kill set for the block.
      DeclSetTy Kill;
      // The set of all variables used in bounds expr for each ntptr in the
      // block.
      BoundsVarTy BoundsVars;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}
    };

    // BlockMapTy stores the mapping from CFGBlocks to ElevatedCFGBlocks.
    using BlockMapTy = llvm::DenseMap<const CFGBlock *, ElevatedCFGBlock *>;
    // A queue of unique ElevatedCFGBlocks to run the dataflow analysis on.
    using WorkListTy = QueueSet<ElevatedCFGBlock>;

  public:
    BoundsAnalysis(Sema &S, CFG *Cfg) : S(S), Cfg(Cfg), Ctx(S.Context) {}

    // Run the dataflow analysis to widen bounds for ntptr's.
    void WidenBounds();

    // Get the widened bounds for block B.
    // @param[in] B is the block for which the widened bounds are needed.
    // @return Widened bounds for ntptrs in block B.
    BoundsMapTy GetWidenedBounds(const CFGBlock *B);

    // Pretty print the widen bounds analysis.
    // @param[in] FD is used to extract the name of the current function for
    // printing.
    void DumpWidenedBounds(FunctionDecl *FD);

  private:
    // Compute Gen set for each edge in the CFG. If there is an edge B1->B2 and
    // the edge condition is of the form "if (*(p + i))" then Gen[B1] = {B2,
    // p:i} . The actual computation of i is done in FillGenSet.
    // @param[in] BlockMap is the map from CFGBlock to ElevatedCFGBlock. Used
    // to lookup ElevatedCFGBlock from CFGBlock.
    void ComputeGenSets(BlockMapTy BlockMap);

    // Compute Kill set for each block in BlockMap. For a block B, a variable V
    // is added to Kill[B] if V is assigned to in B.
    // @param[in] BlockMap is the map from CFGBlock to ElevatedCFGBlock. Used
    // to lookup ElevatedCFGBlock from CFGBlock.
    void ComputeKillSets(BlockMapTy BlockMap);

    // Compute In set for each block in BlockMap. In[B1] = n Out[B*->B1], where
    // B* are all preds of B1.
    // @param[in] EB is the block to compute the In set for.
    // @param[in] BlockMap is the map from CFGBlock to ElevatedCFGBlock. Used
    // to lookup ElevatedCFGBlock from CFGBlock.
    void ComputeInSets(ElevatedCFGBlock *EB, BlockMapTy BlockMap);

    // Compute Out set for each outgoing edge of EB. If the Out set on any edge
    // of EB changes then the successor of EB on that edge is added to
    // Worklist.
    // @param[in] EB is the block to compute the Out set for.
    // @param[in] BlockMap is the map from CFGBlock to ElevatedCFGBlock. Used
    // to lookup ElevatedCFGBlock from CFGBlock.
    // @param[out] The successors of EB are added to WorkList if the Out set of
    // EB changes.
    void ComputeOutSets(ElevatedCFGBlock *EB, BlockMapTy BlockMap,
                        WorkListTy &Worklist);

    // Perform checks, handles conditional expressions, extracts the
    // ntptr offset and fills the Gen set for the edge.
    // @param[in] E is the expr possibly containing the deref of an ntptr. If E
    // contains a pointer deref, the Gen set for the edge EB->SuccEB is
    // updated.
    // @param[in] Source block for the edge for which the Gen set is updated.
    // @param[in] Dest block for the edge for which the Gen set is updated.
    void FillGenSet(Expr *E, ElevatedCFGBlock *EB, ElevatedCFGBlock *SuccEB);

    // Fill Gen set for ntptr derefs.
    // @param[in] E is the expr containing the deref of an ntptr. The Gen set
    // for the edge EB->SuccEB is updated.
    // @param[in] Source block for the edge for which the Gen set is updated.
    // @param[in] Dest block for the edge for which the Gen set is updated.
    void HandlePointerDeref(Expr *E, ElevatedCFGBlock *EB,
                            ElevatedCFGBlock *SuccEB);

    // Fill Gen set for ntptr subscripts.
    // @param[in] E is the expr containing the ntptr subscript. The Gen set
    // for the edge EB->SuccEB is updated.
    // @param[in] Source block for the edge for which the Gen set is updated.
    // @param[in] Dest block for the edge for which the Gen set is updated.
    void HandleArraySubscript(Expr *E, ElevatedCFGBlock *EB,
                              ElevatedCFGBlock *SuccEB);

    // Collect all variables used in bounds expr E.
    // @param[in] E represents the bounds expr for an ntptr.
    // @param[out] BoundsVars is a set of all variables used in the bounds expr
    // E.
    void CollectBoundsVars(const Expr *E, DeclSetTy &BoundsVars);

    // Collect the variables assigned to in a block.
    // @param[in] S is an assignment statement.
    // @param[in] EB is used to access the BoundsVars for the block.
    // @param[out] DefinedVars is the set of all ntptrs whose widened bounds
    // are no longer valid as the ntptr has been assigned to, and hence it must
    // be added to the Kill set of the block.
    void CollectDefinedVars(const Stmt *S, ElevatedCFGBlock *EB,
                            DeclSetTy &DefinedVars);

    // Assign the widened bounds from the ElevatedBlock to the CFG Block.
    // @param[in] BlockMap is the map from CFGBlock to ElevatedCFGBlock. Used
    // to associate the widened bounds from the ElevatedCFGBlock to the CFGBlock.
    void CollectWidenedBounds(BlockMapTy BlockMap);

    // Get the terminating condition for a block. This could be an if condition
    // of the form "if(*(p + i))".
    // @param[in] B is the block for which we need the terminating condition.
    // @return Expression for the terminating condition of block B.
    Expr *GetTerminatorCondition(const CFGBlock *B) const;

    // Check if E is a pointer dereference.
    // @param[in] E is the expression for possibly a pointer deref.
    // @return Whether E is a pointer deref.
    bool IsPointerDerefLValue(Expr *E) const;

    // Check if E contains a pointer dereference.
    // @param[in] E is the expression which possibly contains a pointer deref.
    // @return Whether E contains a pointer deref.
    bool ContainsPointerDeref(Expr *E) const;

    // Check if E contains an array subscript operation.
    // @param[in] E is the expression which possibly contains an array
    // subscript.
    // @return Whether E contains an array subscript.
    bool ContainsArraySubscript(Expr *E) const;

    // WidenedBounds is a DenseMap and hence is not suitable for iteration as
    // its iteration order is non-deterministic. So we first need to order the
    // blocks.
    // @return Blocks ordered by block numbers from higher to lower since block
    // numbers decrease from entry to exit.
    OrderedBlocksTy GetOrderedBlocks();

    // Strip E of all casts.
    // @param[in] E is the expression which must be stripped off of all casts.
    // @return Expr stripped off of all casts.
    Expr *IgnoreCasts(Expr *E);

    // Check if the declared bounds of p are zero. ie: the upper bound of p is
    // equal to p.
    // @param[in] E is the bounds expression for V.
    // @param[in] V is the ntptr.
    // @return Whether the declared bounds of p are zero.
    bool AreDeclaredBoundsZero(const Expr *E, const Expr *V);

    // We do not want to run dataflow analysis on null, entry or exit blocks.
    // So we skip them.
    // @param[in] B is the block which may need to the skipped from dataflow
    // analysis.
    // @return Whether B should be skipped.
    bool SkipBlock(const CFGBlock *B) const;

    // Compute the intersection of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The intersection of sets A and B.
    template<class T> T Intersect(T &A, T &B) const;

    // Compute the union of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The union of sets A and B.
    template<class T> T Union(T &A, T &B) const;

    // Compute the set difference of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The set difference of sets A and B.
    template<class T, class U> T Difference(T &A, U &B) const;

    // Check whether the sets A and B differ.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return Whether sets A and B differ.
    template<class T> bool Differ(T &A, T &B) const;
  };
}

#endif
