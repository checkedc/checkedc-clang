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
  // BoundsMapTy maps a variable that is a pointer to a null-terminated arra to
  // its bounds expression.
  using BoundsMapTy = llvm::DenseMap<const VarDecl *, BoundsExpr *>;

  // StmtBoundsMapTy maps each variable that is a pointer to a null-terminated
  // array that occurs in a statement to its bounds expression.
  using StmtBoundsMapTy = llvm::DenseMap<const Stmt *, BoundsMapTy>;

  // StmtVarSetTy denotes a set of variables that are pointers to
  // null-terminated arrays and that are associated with a statement. The set
  // of variables whose bounds are killed by a statement has the type
  // StmtVarSetTy.
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
      // The In, Out and Gen sets for a block.
      BoundsMapTy In, Out, Gen;
      // The Kill set for a block.
      VarSetTy Kill;
      // The In and Gen sets for each statement in a block.
      StmtBoundsMapTy StmtIn, StmtGen;
      // The Kill set for each statement in a block.
      StmtVarSetTy StmtKill;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}
    }; // end class ElevatedCFGBlock

  private:
    // BlockMapTy denotes the mapping from CFGBlocks to ElevatedCFGBlocks.
    using BlockMapTy = llvm::DenseMap<const CFGBlock *, ElevatedCFGBlock *>;

    // BlockMap maps a CFGBlock to an ElevatedCFGBlock. Given a CFGBlock it is
    // used to lookup an ElevatedCFGBlock.
    BlockMapTy BlockMap;
  
  public:
    BoundsWideningAnalysis(Sema &SemaRef, CFG *Cfg, BoundsVarsTy &BoundsVars) :
      SemaRef(SemaRef), Cfg(Cfg), Ctx(SemaRef.Context),
      BoundsVars(BoundsVars), Lex(Lexicographic(Ctx, nullptr)),
      OS(llvm::outs()) {}

    // Run the dataflow analysis to widen bounds for null-terminated arrays.
    // @param[in] FD is the current function.
    // @param[in] NestedStmts is a set of top-level statements that are
    // nested in another top-level statement.
    void WidenBounds(FunctionDecl *FD, StmtSetTy NestedStmts);

  private:
    // Compute the Kill and Gen sets for the block.
    // @param[in] EB is the current ElevatedCFGBlock.
    void ComputeGenKillSets(ElevatedCFGBlock *EB);

    // Compute the StmtGen and StmtKill sets for a statement in a block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    // @param[in] IsLastStmt indicates whether S is the last statement in the
    // block.
    void ComputeStmtGenKillSets(ElevatedCFGBlock *EB, const Stmt *S,
                                bool IsLastStmt);

    // Update the Gen and Kill sets for the block after computing the StmtGen
    // and StmtKill for the current statement.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    void UpdateBlockGenKillSets(ElevatedCFGBlock *EB, const Stmt *S);

    // Fill the StmtKill set when a variable occurring in the bounds expression
    // of a null-terminated array is modified.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    void FillStmtKillSetForModifiedVars(ElevatedCFGBlock *EB, const Stmt *S);

    // Fill StmtGen and StmtKill sets for the bounds declaration of a
    // null-terminated array.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    // @param[in] V is a variable that is a pointer to a null-terminated array.
    void FillStmtGenKillSetsForBoundsDecl(ElevatedCFGBlock *EB, const Stmt *S,
                                          const VarDecl *V);

    // Fill StmtGen and StmtKill sets for bounds declaration in a where clause.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    // @param[in] WC is the where clause that annotates S.
    void FillStmtGenKillSetsForWhereClause(ElevatedCFGBlock *EB, const Stmt *S,
                                           WhereClause *WC);

    // Fill StmtGen and StmtKill sets for dereference of a variable that is a
    // pointer to a null-terminated array.
    // null-terminated array V at E.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] S is the current statement.
    void FillStmtGenKillSetsForPtrDeref(ElevatedCFGBlock *EB, const Stmt *S);

    // Get the set of variables that can be potentially widened in an
    // expression E.
    // @param[in] E is the given expression.
    // @param[out] VarsToWiden is a set of variables that can be potentially
    // widened in expression E.
    void GetVarsToWiden(Expr *E, VarSetTy &VarsToWiden);

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

    // From the given expression get the dereference expression. A dereference
    // expression can be of the form "*(p + 1)" or "p[1]".
    // @param[in] E is the given expression.
    // @return Returns the dereference expression, if it exists.
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
    // @param[in] B is the block which may need to be skipped from dataflow
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

    // Determine whether sets A and B are equal. Equality is determined by
    // comparing each element in the two input sets.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return Whether sets A and B are equal.
    template<class T> bool IsEqual(T &A, T &B) const;

  }; // end class BoundsWideningAnalysis

} // end namespace clang
#endif
