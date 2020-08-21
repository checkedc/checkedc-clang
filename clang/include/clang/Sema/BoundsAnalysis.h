//===---------- BoundsAnalysis.h - Dataflow for bounds widening-----------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
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

  // DeclSetTy denotes a set of VarDecls.
  using DeclSetTy = llvm::DenseSet<const VarDecl *>;

  // A mapping of VarDecl V to all the variables occuring in its bounds
  // expression. This is used to compute Kill sets. An assignment to any
  // variable occuring in the bounds expression of an ntptr kills any computed
  // bounds for that ntptr in that block.
  using BoundsVarTy = llvm::DenseMap<const VarDecl *, DeclSetTy>;

  // OrderedBlocksTy denotes blocks ordered by block numbers. This is useful
  // for printing the blocks in a deterministic order.
  using OrderedBlocksTy = std::vector<const CFGBlock *>;

  // ExprIntPairTy denotes a pair of an expression and an integer constant.
  // This is used as a return type when an expression is split into a base and
  // an offset.
  using ExprIntPairTy = std::pair<const Expr *, llvm::APSInt>;

  // StmtDeclSetTy denotes a mapping between a Stmt and a set of VarDecls. This
  // is used to store the Kill set for a block.
  // A VarDecl V is killed in a Stmt S if:
  // 1. V is assigned to in S, or
  // 2. any variable used in the bounds expr of V is assigned to in S.
  using StmtDeclSetTy = llvm::DenseMap<const Stmt *, DeclSetTy>;

  // StmtSet denotes a set of Stmts.
  using StmtSet = llvm::SmallPtrSet<const Stmt *, 16>;

  class BoundsAnalysis {
  private:
    Sema &S;
    CFG *Cfg;
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;

    class ElevatedCFGBlock {
    public:
      const CFGBlock *Block;
      // The In set for the block.
      BoundsMapTy In;
      // The Gen and Out sets for the block.
      EdgeBoundsTy Gen, Out;
      // The Kill set for the block.
      StmtDeclSetTy Kill;
      // The set of all variables used in bounds expr for each ntptr in the
      // block.
      BoundsVarTy BoundsVars;

      // To compute In[B] we compute the intersection of Out[B*->B], where B*
      // are all preds of B. When there is a back edge from block B' to B (for
      // example in loops), the Out set for block B' will be empty when we
      // first enter B. As a result, the intersection operation would always
      // result in an empty In set for B.

      // So to handle this, we consider the In and Out sets for all blocks to
      // have a default value of "Top" which indicates a set of all members of
      // the Gen set. In this way we ensure that the intersection does not
      // result in an empty set even if the Out set for a block is actually
      // empty.

      // But we also need to handle the case where there is an unconditional
      // jump into a block (for example, as a result of a goto). In this case,
      // we cannot widen the bounds because we would not have checked for the
      // ptr dereference. So in this case we want the intersection to result in
      // an empty set.

      // So we mark the In and Out sets of the Entry block as "empty".
      // IsInSetEmpty and IsOutSetEmpty indicate whether the In and Out sets
      // for a block have been marked as "empty".
      bool IsInSetEmpty;
      llvm::DenseMap<const CFGBlock *, bool> IsOutSetEmpty;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}
    };

    // BlockMapTy stores the mapping from CFGBlocks to ElevatedCFGBlocks.
    using BlockMapTy = llvm::DenseMap<const CFGBlock *, ElevatedCFGBlock *>;
    // A queue of unique ElevatedCFGBlocks to run the dataflow analysis on.
    using WorkListTy = QueueSet<ElevatedCFGBlock>;

    // BlockMap is the map from CFGBlock to ElevatedCFGBlock. Used
    // to lookup ElevatedCFGBlock from CFGBlock.
    BlockMapTy BlockMap;

    // A set of all ntptrs in scope. Currently, we simply collect all ntptrs
    // defined in the function.
    DeclSetTy NtPtrsInScope;

    // To compute In[B] we compute the intersection of Out[B*->B], where B* are
    // all preds of B. When there is a back edge from block B' to B (for
    // example in loops), the Out set for block B' will be empty when we first
    // enter B. As a result, the intersection operation would always result in
    // an empty In set for B.

    // So to handle this, we consider the In and Out sets for all blocks to
    // have a default value of "Top" which indicates a set of all members of
    // the Gen set. In this way we ensure that the intersection does not result
    // in an empty set even if the Out set for a block is actually empty.

    // But we also need to handle the case where there is an unconditional jump
    // into a block (for example, as a result of a goto). In this case, we
    // cannot widen the bounds because we would not have checked for the ptr
    // dereference. So in this case we want the intersection to result in an
    // empty set.

    // So we initialize the In and Out sets of all blocks, except the Entry
    // block, as "Top".
    BoundsMapTy Top;

  public:
    BoundsAnalysis(Sema &S, CFG *Cfg) :
      S(S), Cfg(Cfg), Ctx(S.Context),
      Lex(Lexicographic(Ctx, nullptr)), OS(llvm::outs()) {}

    // Run the dataflow analysis to widen bounds for ntptr's.
    // @param[in] FD is the current function.
    // @param[in] NestedStmts is a set of top-level statements that are
    // nested in another top-level statement.
    void WidenBounds(FunctionDecl *FD, StmtSet NestedStmts);

    // Get the widened bounds for block B.
    // @param[in] B is the block for which the widened bounds are needed.
    // @return Widened bounds for ntptrs in block B.
    BoundsMapTy GetWidenedBounds(const CFGBlock *B);

    // Pretty print the widen bounds analysis.
    // printing.
    // @param[in] FD is the current function.
    void DumpWidenedBounds(FunctionDecl *FD);

    // Get the Kill set for the current block. The Kill set is a mapping of
    // Stmts to variables whose bounds are killed by each Stmt in the block.
    // Note: This method is intended to be invoked from CheckBoundsDeclaration
    // or a similar place which does bounds inference/checking.
    // @param[in] B is the current CFGBlock.
    // return A mapping of Stmts to variables whose bounds are killed by the
    // Stmt.
    StmtDeclSetTy GetKilledBounds(const clang::CFGBlock *B);

  private:
    // Compute Gen set for each edge in the CFG. If there is an edge B1->B2 and
    // the edge condition is of the form "if (*(p + i))" then Gen[B1] = {B2,
    // p:i} . The actual computation of i is done in FillGenSet.
    void ComputeGenSets();

    // Compute Kill set for each block in BlockMap. For a block B, if a
    // variable V is assigned to in B by Stmt S, then the pair S:V is added to
    // the Kill set for the block.
    // @param[in] NestedStmts is a set of top-level statements that are
    // nested in another top-level statement.
    void ComputeKillSets(StmtSet NestedStmts);

    // Compute In set for each block in BlockMap. In[B1] = n Out[B*->B1], where
    // B* are all preds of B1.
    // @param[in] EB is the block to compute the In set for.
    void ComputeInSets(ElevatedCFGBlock *EB);

    // Compute Out set for each outgoing edge of EB. If the Out set on any edge
    // of EB changes then the successor of EB on that edge is added to
    // Worklist.
    // @param[in] EB is the block to compute the Out set for.
    // @param[out] The successors of EB are added to WorkList if the Out set of
    // EB changes.
    void ComputeOutSets(ElevatedCFGBlock *EB, WorkListTy &Worklist);

    // Perform checks, handles conditional expressions, extracts the
    // ntptr offset and fills the Gen set for the edge.
    // @param[in] E is the expr possibly containing the deref of an ntptr. If E
    // contains a pointer deref, the Gen set for the edge EB->SuccEB is
    // updated.
    // @param[in] Source block for the edge for which the Gen set is updated.
    // @param[in] Dest block for the edge for which the Gen set is updated.
    void FillGenSet(Expr *E, ElevatedCFGBlock *EB, ElevatedCFGBlock *SuccEB);

    // Uniformize the expr, fill Gen set and get variables used in bounds expr
    // for the ntptr.
    // @param[in] E is an ntptr dereference or array subscript expr.
    // @param[in] Source block for the edge for which the Gen set is updated.
    // @param[in] Dest block for the edge for which the Gen set is updated.
    void FillGenSetAndGetBoundsVars(const Expr *E,
                                    ElevatedCFGBlock *EB,
                                    ElevatedCFGBlock *SuccEB);

    // Collect all variables used in bounds expr E.
    // @param[in] E represents the bounds expr for an ntptr.
    // @param[out] BoundsVars is a set of all variables used in the bounds expr
    // E.
    void CollectBoundsVars(const Expr *E, DeclSetTy &BoundsVars);

    // Assign the widened bounds from the ElevatedBlock to the CFG Block.
    void CollectWidenedBounds();

    // Get the terminating condition for a block. This could be an if condition
    // of the form "if(*(p + i))".
    // @param[in] B is the block for which we need the terminating condition.
    // @return Expression for the terminating condition of block B.
    Expr *GetTerminatorCondition(const CFGBlock *B) const;

    // Check if V is an _Nt_array_ptr or an _Nt_checked array.
    // @param[in] V is the VarDecl.
    // @return Whether V is an _Nt_array_ptr or an _Nt_checked array.
    bool IsNtArrayType(const VarDecl *V) const;

    // WidenedBounds is a DenseMap and hence is not suitable for iteration as
    // its iteration order is non-deterministic. So we first need to order the
    // blocks.
    // @return Blocks ordered by block numbers from higher to lower since block
    // numbers decrease from entry to exit.
    OrderedBlocksTy GetOrderedBlocks();

    // Invoke IgnoreValuePreservingOperations to strip off casts.
    // @param[in] E is the expression whose casts must be stripped.
    // @return E with casts stripped off.
    Expr *IgnoreCasts(const Expr *E);

    // We do not want to run dataflow analysis on null or exit blocks. So we
    // skip them.
    // @param[in] B is the block which may need to the skipped from dataflow
    // analysis.
    // @return Whether B should be skipped.
    bool SkipBlock(const CFGBlock *B) const;

    // Get the DeclRefExpr from an expression E.
    // @param[in] An expression E which is known to be either an LValueToRValue
    // cast or an ArrayToPointerDecay cast.
    // @return The DeclRefExpr from the expression E.
    DeclRefExpr *GetDeclOperand(const Expr *E);

    // A DeclRefExpr can be a reference either to an array subscript (in which
    // case it is wrapped around a ArrayToPointerDecay cast) or to a pointer
    // dereference (in which case it is wrapped around an LValueToRValue cast).
    // @param[in] An expression E.
    // @return Whether E is an expression containing a reference to an array
    // subscript or a pointer dereference.
    bool IsDeclOperand(const Expr *E);

    // Make an expression uniform by moving all DeclRefExpr to the LHS and all
    // IntegerLiterals to the RHS.
    // @param[in] E is the expression which should be made uniform.
    // @return A pair of an expression and an integer constant. The expression
    // contains all DeclRefExprs of E and the integer constant contains all
    // IntegerLiterals of E.
    ExprIntPairTy SplitIntoBaseOffset(const Expr *E);

    // Collect all ntptrs in scope. Currently, this simply collects all ntptrs
    // defined in all blocks in the current function. This function inserts the
    // VarDecls for the ntptrs in NtPtrsInScope.
    // @param[in] FD is the current function.
    void CollectNtPtrsInScope(FunctionDecl *FD);

    // If variable V is killed by Stmt S in Block B, add TopLevelStmt:V pair
    // to EB->Kill, where TopLevelStmt is the top-level Stmt that contains S.
    // @param[in] EB is the ElevatedCFGBlock for the current block.
    // @param[in] TopLevelStmt is the top-level Stmt in the block.
    // @param[in] S is the current Stmt in the block.
    void FillKillSet(ElevatedCFGBlock *EB, const Stmt *TopLevelStmt, const Stmt *S);

    // Initialize the In and Out sets for all blocks, except the Entry block,
    // as Top.
    void InitInOutSets();

    // Check if the switch case label is null.
    // @param[in] EB is the ElevatedCFGBlock for the current block.
    bool CheckIsSwitchCaseNull(ElevatedCFGBlock *EB);

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
