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
#include "clang/AST/ExprUtils.h"
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
 
  // StmtSetTy denotes a set of statements.
  using StmtSetTy = llvm::SmallPtrSet<const Stmt *, 16>;

  // ExprVarsTy maps an expression to a set of variables. If E is an expression
  // dereferencing a null-terminated array, then ExprVarsTy maps the expression
  // (E + 1) to a set of null-terminated arrays whose bounds may potentially be
  // widened to (E + 1).
  using ExprVarsTy = llvm::DenseMap<Expr *, const VarDecl *>;

  // The BoundsWideningAnalysis class represents the dataflow analysis for
  // bounds widening. The sets In, Out, Gen and Kill that are used by the
  // analysis are members of this class. The class also has methods that act on
  // these sets to perform the dataflow analysis.
  class BoundsWideningAnalysis {
  private:
    Sema &SemaRef;
    CFG *Cfg;
    ASTContext &Ctx;
    BoundsVarsTy &BoundsVars;
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
    }; // end class ElevatedCFGBlock

  private:
    // BlockMapTy denotes the mapping from CFGBlocks to ElevatedCFGBlocks.
    using BlockMapTy = llvm::DenseMap<const CFGBlock *, ElevatedCFGBlock *>;

    // QueueSet is a queue of unique elements. It is used to process
    // ElevatedCFGBlocks for the dataflow analysis.
    using WorkListTy = QueueSet<ElevatedCFGBlock>;

    // BlockMap maps a CFGBlock to an ElevatedCFGBlock. Given a CFGBlock it is
    // used to lookup an ElevatedCFGBlock.
    BlockMapTy BlockMap;

    // Top denotes the initial value of the In and Out sets for all basic
    // blocks, except the Entry block. the In and Out sets for the Entry block
    // are initialized to Bot.
    BoundsMapTy Top, Bot;
  
  public:
    BoundsWideningAnalysis(Sema &SemaRef, CFG *Cfg, BoundsVarsTy &BoundsVars) :
      SemaRef(SemaRef), Cfg(Cfg), Ctx(SemaRef.Context),
      BoundsVars(BoundsVars), Lex(Lexicographic(Ctx, nullptr)),
      OS(llvm::outs()) {}

    // Run the dataflow analysis to widen bounds for null-terminated arrays.
    // @param[in] FD is the current function.
    // @param[in] NestedStmts is a set of top-level statements that are
    // nested in another top-level statement.
    // @param[in] BoundsVars is a map of each variable Z in the current
    // function to the set of all variables in whose bounds expressions Z
    // occurs.
    void WidenBounds(FunctionDecl *FD, StmtSetTy NestedStmts);

  private:
    // Compute the Gen and Kill sets for each statement in a block.
    void ComputeGenKillSets();

    // Compute the Gen sets for all statements in a block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    // @param[in] PrevS is the previous statement of S.
    // @param[in] IsLastStmt indicates whether S is the last statement in the
    // block.
    void ComputeGenSet(ElevatedCFGBlock *EB, const Stmt *S,
                       const Stmt *PrevS, bool IsLastStmt);

    // Fill Gen set with the bounds declared for a null-terminated array.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    // @param[in] V is a null-terminated array variable.
    void FillGenSetForBoundsDecl(ElevatedCFGBlock *EB, const Stmt *S,
                                 const VarDecl *V);

    // Fill Gen set with the bounds declared in a where clause on a statement.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    // @param[in] WC is the where clause that annotates S.
    void FillGenSetForWhereClause(ElevatedCFGBlock *EB, const Stmt *S,
                                  WhereClause *WC);

    // Fill Gen set with V:bounds(lower, E + 1) if a statement dereferences a
    // null-terminated array V at E.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    void FillGenSetForPtrDeref(ElevatedCFGBlock *EB, const Stmt *S);

    // Compute the Kill sets for all statements in a block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    // @param[in] PrevS is the previous statement of S.
    void ComputeKillSet(ElevatedCFGBlock *EB, const Stmt *S,
                        const Stmt *PrevS);

    // Get all variables modfied by statement S or statements nested in S.
    // @param[in] S is a given statement.
    // @param[out] ModifiedVars is a set of variables modified by S or
    // statements nested in S.
    void GetModifiedVars(const Stmt *S, VarSetTy &ModifiedVars);

    // Add an offset to a given expression to get the widened expression.
    // @param[in] E is the given expression.
    // @param[in] Offset is the given offset.
    // @return Returns the expression E + Offset.
    Expr *GetWidenedExpr(Expr *E, unsigned Offset) const;

    // From a given terminating condition extract the terminating condition for
    // the current block. Given an expression like "if (e1 && e2)" this
    // function returns e2 which is the terminating condition for the current
    // block.
    // @param[in] E is given terminating condition.
    // @return The terminating condition for the block.
    Expr *GetTerminatorCondition(const Expr *E) const;

    // Use the last statement in a block to get the terminating condition for
    // the block. This could be an expression of the form "if (e1 && e2)".
    // @param[in] B is the block for which we need the terminating condition.
    // @return Expression for the terminating condition of block B.
    Expr *GetTerminatorCondition(const CFGBlock *B) const;

    // From the given expression get the dereferences expression. A dereference
    // expression can be of the form "*(p + 1)" or "p[1]".
    // @param[in] E is the given expression.
    // return Returns the dereference expression, if it exists.
    Expr *GetDerefExpr(Expr *E) const;

    // Get the variables occurring in an expression.
    // @param[in] E is the given expression.
    // @param[out] VarsInExpr is a set of variables that occur in E.
    void GetVarsInExpr(Expr *E, VarSetTy &VarsInExpr) const;

    // Invoke IgnoreValuePreservingOperations to strip off casts.
    // @param[in] E is the expression whose casts must be stripped.
    // @return E with casts stripped off.
    Expr *IgnoreCasts(const Expr *E) const;

    // We do not want to run dataflow analysis on null blocks or the exit
    // block. So we skip them.
    // @param[in] B is the block which may need to the skipped from dataflow
    // analysis.
    // @return Whether B should be skipped.
    bool SkipBlock(const CFGBlock *B) const;

    // Check if V is an _Nt_array_ptr or an _Nt_checked array.
    // @param[in] V is a VarDecl.
    // @return Whether V is an _Nt_array_ptr or an _Nt_checked array.
    bool IsNtArrayType(const VarDecl *V) const;

    // Compute the set difference of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The set difference of sets A and B.
    template<class T, class U> T Difference(T &A, U &B) const;

    // Compute the intersection of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The intersection of sets A and B.
    template<class T> T Intersect(T &A, T &B) const;

  }; // end class BoundsWideningAnalysis

} // end namespace clang
#endif
