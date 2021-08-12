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
#include "clang/AST/PrettyPrinter.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Sema/BoundsUtils.h"
#include "clang/Sema/CheckedCAnalysesPrepass.h"
#include "clang/Sema/Sema.h"

namespace clang {

  // BoundsMapTy maps a variable that is a pointer to a null-terminated array
  // to its bounds expression.
  using BoundsMapTy = llvm::DenseMap<const VarDecl *, RangeBoundsExpr *>;

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

  // StmtMapTy denotes a map of a statement to another statement. This is used
  // to store the mapping of a statement to its previous statement in a block.
  using StmtMapTy = llvm::DenseMap<const Stmt *, const Stmt *>;

  // ExprVarsTy maps an expression to a set of variables. If E is an expression
  // dereferencing a null-terminated array, then ExprVarsTy maps the expression
  // (E + 1) to a set of null-terminated arrays whose bounds may potentially be
  // widened to (E + 1).
  using ExprVarsTy = llvm::DenseMap<Expr *, const VarDecl *>;

  // OrderedBlocksTy denotes blocks ordered by block numbers. This is useful
  // for printing the blocks in a deterministic order.
  using OrderedBlocksTy = std::vector<const CFGBlock *>;

  // A tuple (Tup) of three elements such that we need to replace Tup[0] with
  // Tup[1] in the bounds of every pointer in Tup[3].
  using LValuesToReplaceInBoundsTy = std::tuple<Expr *, Expr *, VarSetTy>;

  // A mapping of invertible statements to LValuesToReplaceInBoundsTy.
  using InvertibleStmtMapTy = llvm::DenseMap<const Stmt *,
                                             LValuesToReplaceInBoundsTy>;

  // A struct representing various information about the terminating condition
  // of a block.
  struct TermCondInfoTy {
    // The expression that dereferences a pointer or subscripts an array. For
    // example:
    // if (*(p + i) == 0) ==> DerefExpr = p + i
    Expr *DerefExpr;

    // Whether the terminating condition tests for a null value. For example:
    // if (*p == 0)   ==> IsCheckNull = True
    // if (*p != 0)   ==> IsCheckNull = False
    // if (*p == 'a') ==> IsCheckNull = False
    // if (*p != 'a') ==> IsCheckNull = True
    bool IsCheckNull;
  };

} // end namespace clang

namespace clang {

  //===-------------------------------------------------------------------===//
  // Class definition of the BoundsWideningUtil class. This class contains
  // helper methods that are used by the BoundsWideningAnalysis class to
  // perform the dataflow analysis. The BoundsWideningAnalysis class is defined
  // later in this file.
  //===-------------------------------------------------------------------===//

  class BoundsWideningUtil {
  private:
    Sema &SemaRef;
    CFG *Cfg;
    ASTContext &Ctx;
    Lexicographic Lex;
    BoundsVarsTy &BoundsVarsLower;
    BoundsVarsTy &BoundsVarsUpper;

  public:
    BoundsWideningUtil(Sema &SemaRef, CFG *Cfg,
                       ASTContext &Ctx, Lexicographic Lex,
                       BoundsVarsTy &BoundsVarsLower,
                       BoundsVarsTy &BoundsVarsUpper) :
      SemaRef(SemaRef), Cfg(Cfg), Ctx(Ctx), Lex(Lex),
      BoundsVarsLower(BoundsVarsLower), BoundsVarsUpper(BoundsVarsUpper) {}

    // Check if B2 is a subrange of B1.
    // @param[in] B1 is the first range.
    // @param[in] B2 is the second range.
    // @return Returns true if B2 is a subrange of B1, false otherwise.
    bool IsSubRange(RangeBoundsExpr *B1, RangeBoundsExpr *B2) const;

    // Determine if the switch-case has a case label (other than default) that
    // tests for null.
    // @param[in] CurrBlock is the current block.
    // @return Returns true if the switch-case has a label that tests for null.
    bool ExistsNullCaseLabel(const CFGBlock *CurrBlock) const;

    // Determine if the current block begins a case of a switch-case.
    // @param[in] CurrBlock is the current block.
    // @return Returns true if the current block begins a case.
    bool IsSwitchCaseBlock(const CFGBlock *CurrBlock,
                           const CFGBlock *PredBlock) const;

    // Determine if the switch-case label on the current block tests for null.
    // @param[in] CurrBlock is the current block.
    // @return Returns true if the case label on the current block tests for
    // null.
    bool CaseLabelTestsForNull(const CFGBlock *CurrBlock) const;

    // Determine if the edge from PredBlock to CurrBlock is a fallthrough.
    // @param[in] PredBlock is a predecessor block of the current block.
    // @param[in] CurrBlock is the current block.
    // @return Returns true if the edge is a fallthrough, false otherwise.
    bool IsFallthroughEdge(const CFGBlock *PredBlock,
                           const CFGBlock *CurrBlock) const;

    // Determine if the edge from PredBlock to CurrBlock is a true edge.
    // @param[in] PredBlock is a predecessor block of the current block.
    // @param[in] CurrBlock is the current block.
    // @return Returns true if the edge is a true edge, false otherwise.
    bool IsTrueEdge(const CFGBlock *PredBlock,
                    const CFGBlock *CurrBlock) const;

    // Get all variables modified by CurrStmt or statements nested in CurrStmt.
    // @param[in] CurrStmt is a given statement.
    // @param[out] ModifiedVars is a set of variables modified by CurrStmt or
    // statements nested in CurrStmt.
    void GetModifiedVars(const Stmt *CurrStmt, VarSetTy &ModifiedVars) const;

    // Get the set of variables that are pointers to null-terminated arrays and
    // in whose lower bounds expressions the variables in Vars occur.
    // @param[in] Vars is a set of variables.
    // @param[out] PtrsWithVarsInLowerBounds is a set of variables that are
    // pointers to null-terminated arrays and in whose lower bounds expressions
    // the variables in Vars occur.
    void GetPtrsWithVarsInLowerBounds(
      VarSetTy &Vars, VarSetTy &PtrsWithVarsInLowerBounds) const;

    // Get the set of variables that are pointers to null-terminated arrays and
    // in whose upper bounds expressions the variables in Vars occur.
    // @param[in] Vars is a set of variables.
    // @param[out] PtrsWithVarsInLowerBounds is a set of variables that are
    // pointers to null-terminated arrays and in whose upper bounds expressions
    // the variables in Vars occur.
    void GetPtrsWithVarsInUpperBounds(
      VarSetTy &Vars, VarSetTy &PtrsWithVarsInUpperBounds) const;

    // Add an offset to a given expression.
    // @param[in] E is the given expression.
    // @param[in] Offset is the given offset.
    // @return Returns the expression E + Offset.
    Expr *AddOffsetToExpr(Expr *E, unsigned Offset) const;

    // Get various information about the terminating condition of a block.
    // @param[in] TermCond is the terminating condition of a block.
    // @return A struct containing various information about the terminating
    // condition.
    TermCondInfoTy GetTermCondInfo(const Expr *TermCond) const;

    // Fill the TermCondInfo parameter with information about the terminating
    // condition TermCond.
    // @param[in] TermCond is the terminating condition of a block.
    // @param[out] TermCondInfo is the struct that is filled with various
    // information about the terminating condition.
    void FillTermCondInfo(const Expr *TermCond,
                          TermCondInfoTy &TermCondInfo) const;

    // Get the variable in an expression that is a pointer to a null-terminated
    // array.
    // @param[in] E is the given expression.
    // @return The variable in the expression that is a pointer to a
    // null-terminated array; nullptr if no such variable exists in the
    // expression.
    const VarDecl *GetNullTermPtrInExpr(Expr *E) const;

    // Invoke IgnoreValuePreservingOperations to strip off casts.
    // @param[in] E is the expression whose casts must be stripped.
    // @return E with casts stripped off.
    Expr *IgnoreCasts(const Expr *E) const;

    // Strip off more casts than IgnoreCasts.
    // @param[in] E is the expression whose casts must be stripped.
    // @return E with casts stripped off.
    Expr *StripCasts(const Expr *E) const;

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
    template<class T, class U>
    T Difference(T &A, U &B) const;

    // Compute the intersection of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The intersection of sets A and B.
    template<class T>
    T Intersect(T &A, T &B) const;

    // Compute the union of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The union of sets A and B.
    template<class T>
    T Union(T &A, T &B) const;

    // Determine whether sets A and B are equal. Equality is determined by
    // comparing each element in the two input sets.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return Whether sets A and B are equal.
    template<class T>
    bool IsEqual(T &A, T &B) const;

  }; // end of BoundsWideningUtil class.

  // Note: Template specializations of a class member must be present at the
  // same namespace level as the class. So we need to declare template
  // specializations outside the class declaration.

  // Template specialization for computing the difference between BoundsMapTy
  // and VarSetTy.
  template<>
  BoundsMapTy BoundsWideningUtil::Difference<BoundsMapTy, VarSetTy>(
    BoundsMapTy &A, VarSetTy &B) const;

  // Template specialization for computing the union of BoundsMapTy.
  template<>
  BoundsMapTy BoundsWideningUtil::Union<BoundsMapTy>(
    BoundsMapTy &A, BoundsMapTy &B) const;

  // Template specialization for computing the intersection of BoundsMapTy.
  template<>
  BoundsMapTy BoundsWideningUtil::Intersect<BoundsMapTy>(
    BoundsMapTy &A, BoundsMapTy &B) const;

  // Template specialization for determining the equality of BoundsMapTy.
  template<>
  bool BoundsWideningUtil::IsEqual<BoundsMapTy>(
    BoundsMapTy &A, BoundsMapTy &B) const;

} // end namespace clang

namespace clang {
  //===-------------------------------------------------------------------===//
  // Implementation of the methods in the BoundsWideningAnalysis class. This is
  // the main class that implements the dataflow analysis for bounds widening
  // of null-terminated arrays. The BoundsWideningAnalysis class represents the
  // dataflow analysis for bounds widening. The sets In, Out, Gen and Kill that
  // are used by the analysis are members of this class. The class also has
  // methods that act on these sets to perform the dataflow analysis. This
  // class uses helper methods from the BoundsWideningUtil class that are
  // defined later in this file.
  //===-------------------------------------------------------------------===//

  class BoundsWideningAnalysis {
  private:
    Sema &SemaRef;
    CFG *Cfg;
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;
    BoundsWideningUtil BWUtil;

    class ElevatedCFGBlock {
    public:
      const CFGBlock *Block;
      // The In and Out sets for a block.
      BoundsMapTy In, Out;

      // The Gen set for each statement in a block.
      StmtBoundsMapTy StmtGen;

      // The Kill set for each statement in a block.
      StmtVarSetTy StmtKill;

      // A mapping from a statement to its previous statement in a block.
      StmtMapTy PrevStmtMap;

      // The last statement of the block. This is nullptr if the block is empty.
      const Stmt *LastStmt = nullptr;

      // Various information about the terminating condition of the block.
      TermCondInfoTy TermCondInfo;

      // The In set of the last statment of each block.
      BoundsMapTy InOfLastStmt;

      // The Out set of the previous statement of a statement in a block.
      BoundsMapTy OutOfPrevStmt;

      // This stores the adjusted bounds after we have determined the
      // invertibility of the current statement that modifies variables
      // occurring in bounds expressions.
      StmtBoundsMapTy AdjustedBounds;

      // A mapping of invertible statements to LValuesToReplaceInBoundsTy.
      InvertibleStmtMapTy InvertibleStmts;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}

    }; // end of ElevatedCFGBlock class.

  private:

    // BlockMapTy denotes the mapping from CFGBlocks to ElevatedCFGBlocks.
    using BlockMapTy = llvm::DenseMap<const CFGBlock *, ElevatedCFGBlock *>;

    // A queue of unique ElevatedCFGBlocks involved in the fixpoint of the
    // dataflow analysis.
    using WorkListTy = QueueSet<ElevatedCFGBlock>;

    // BlockMap maps a CFGBlock to an ElevatedCFGBlock. Given a CFGBlock it is
    // used to lookup an ElevatedCFGBlock.
    BlockMapTy BlockMap;

    // AllNullTermPtrsInFunc denotes all variables in the function that are
    // pointers to null-terminated arrays.
    VarSetTy AllNullTermPtrsInFunc;
  
  public:
    // Top is a special bounds expression that denotes the super set of all
    // bounds expressions.
    static constexpr RangeBoundsExpr *Top = nullptr;

    BoundsWideningAnalysis(Sema &SemaRef, CFG *Cfg,
                           BoundsVarsTy &BoundsVarsLower,
                           BoundsVarsTy &BoundsVarsUpper) :
      SemaRef(SemaRef), Cfg(Cfg), Ctx(SemaRef.Context),
      Lex(Lexicographic(Ctx, nullptr)), OS(llvm::outs()),
      BWUtil(BoundsWideningUtil(SemaRef, Cfg, Ctx, Lex,
                                BoundsVarsLower, BoundsVarsUpper)) {}

    // Run the dataflow analysis to widen bounds for null-terminated arrays.
    // @param[in] FD is the current function.
    // @param[in] NestedStmts is a set of top-level statements that are nested
    // in another top-level statement.
    void WidenBounds(FunctionDecl *FD, StmtSetTy NestedStmts);

    // Pretty print the widened bounds for all null-terminated arrays in the
    // current function.
    // @param[in] FD is the current function.
    // @param[in] PrintOption == 0: Dump widened bounds
    //            PrintOption == 1: Dump dataflow sets for bounds widening
    void DumpWidenedBounds(FunctionDecl *FD, int PrintOption);

    // Pretty print a container that maps variables to their bounds
    // expressions.
    // @param[in] BoundsMap is a map that maps variables to their bounds
    // expressions.
    // @param[in] EmptyMessage is the message displayed if the container is
    // empty.
    // @param[in] PrintOption == 0: Dump widened bounds
    //            PrintOption == 1: Dump dataflow sets for bounds widening
    void PrintBoundsMap(BoundsMapTy BoundsMap, int PrintOption) const;

    // Pretty print a set of variables.
    // @param[in] VarSet is a set of variables.
    // @param[in] EmptyMessage is the message displayed if the container is
    // empty.
    // @param[in] PrintOption == 0: Dump widened bounds
    //            PrintOption == 1: Dump dataflow sets for bounds widening
    void PrintVarSet(VarSetTy VarSet, int PrintOption) const;

    // Pretty print a statement.
    // @param[in] CurrStmt is the statement to be printed.
    void PrintStmt(const Stmt *CurrStmt) const;

    // Get the Out set for the statement. This set represents the bounds
    // widened after the statement.
    // Note: This method can be called from outside this class to get the
    // widened bounds after a statement.
    // @param[in] B is the current block.
    // @param[in] CurrStmt is the current statement.
    BoundsMapTy GetStmtOut(const CFGBlock *B, const Stmt *CurrStmt) const;

    // Get the In set for the statement. This set represents the bounds
    // widened before the statement.
    // Note: This method can be called from outside this class to get the
    // widened bounds before a statement.
    // @param[in] B is the current block.
    // @param[in] CurrStmt is the current statement.
    BoundsMapTy GetStmtIn(const CFGBlock *B, const Stmt *CurrStmt) const;

    // Get the bounds that are widened in the current block before the current
    // statement and that are not killed by the current statement.
    // Note: This method can be called from outside this class and can be used
    // to control diagnostics for observed bounds.
    // @param[in] B is the current block.
    // @param[in] CurrStmt is the current statement.
    BoundsMapTy GetBoundsWidenedAndNotKilled(const CFGBlock *B,
                                             const Stmt *CurrStmt) const;

  private:
    // Compute Gen and Kill sets for the block and statements in the block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] NestedStmts is a set of top-level statements that are
    // nested in another top-level statement.
    void ComputeGenKillSets(ElevatedCFGBlock *EB, StmtSetTy NestedStmts);

    // Compute the StmtGen and StmtKill sets for a statement in a block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] CurrStmt is the current statement.
    // @param[in] NestedStmts is a set of top-level statements that are
    // nested in another top-level statement.
    void ComputeStmtGenKillSets(ElevatedCFGBlock *EB, const Stmt *CurrStmt,
                                StmtSetTy NestedStmts);

    // Compute the In set for the block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @return Return true if the In set of the block has changed, false
    // otherwise.
    bool ComputeInSet(ElevatedCFGBlock *EB);

    // Compute the Out set for the block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @return Return true if the Out set of the block has changed, false
    // otherwise.
    bool ComputeOutSet(ElevatedCFGBlock *EB);

    // Get the Out set for the last statement of a block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @return Return the Out set for the last statement of EB. If EB does not
    // contain any statement then return the In set for EB.
    BoundsMapTy GetOutOfLastStmt(ElevatedCFGBlock *EB) const;

    // Initialize the In and Out sets for the block.
    // @param[in] FD is the current function.
    // @param[in] EB is the current ElevatedCFGBlock.
    void InitBlockInOutSets(FunctionDecl *FD, ElevatedCFGBlock *EB);

    // Add the successors of the current block to WorkList.
    // @param[in] CurrBlock is the current block.
    // @param[in] WorkList stores the blocks remaining to be processed for the
    // fixpoint computation.
    void AddSuccsToWorkList(const CFGBlock *CurrBlock, WorkListTy &WorkList);

    // Prune the Out set of the pred block according to various conditions.
    // @param[in] PredEB is a predecessor block of the current block.
    // @param[in] CurrEB is the current block.
    // @return The pruned Out set for PredEB.
    BoundsMapTy PruneOutSet(ElevatedCFGBlock *PredEB,
                            ElevatedCFGBlock *CurrEB) const;

    // Initialize the list of variables that are pointers to null-terminated
    // arrays to the null-terminated arrays that are passed as parameters to
    // the function. This function updates the AllNullTermPtrsInFunc set.
    // @param[in] FD is the current function.
    void InitNullTermPtrsInFunc(FunctionDecl *FD);

    // Update the list of variables that are pointers to null-terminated arrays
    // with the variables that are in StmtGen for the current statement in the
    // block. This function updates the AllNullTermPtrsInFunc set.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] CurrStmt is the current statement.
    void UpdateNullTermPtrsInFunc(ElevatedCFGBlock *EB, const Stmt *CurrStmt);

    // Fill the Gen and Kill sets for a statement using the variable and bounds
    // expressions in VarsAndBounds.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] CurrStmt is the current statement.
    // @param[in] VarsAndBounds is a map of variables to their bounds
    // expressions.
    void FillStmtGenKillSets(ElevatedCFGBlock *EB, const Stmt *CurrStmt,
                             BoundsMapTy &VarsAndBounds);

    // Get the mapping of variables to their bounds expressions in the bounds
    // declaration of a null-terminated array.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] V is a variable that is a pointer to a null-terminated array.
    // @param[out] VarsAndBounds is a map of variables to their bounds
    // expressions. This field is updated by this function.
    void GetVarsAndBoundsInDecl(ElevatedCFGBlock *EB, const VarDecl *V,
                                BoundsMapTy &VarsAndBounds);

    // Get the mapping of variables to their bounds expressions in a where
    // clause.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] WC is the where clause that annotates CurrStmt.
    // @param[out] VarsAndBounds is a map of variables to their bounds
    // expressions. This field is updated by this function.
    void GetVarsAndBoundsInWhereClause(ElevatedCFGBlock *EB,
                                       WhereClause *WC,
                                       BoundsMapTy &VarsAndBounds);

    // Get the mapping of variables to their bounds expressions from an
    // expression that dereferences a null-terminated array.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[out] VarsAndBounds is a map of variables to their bounds
    // expressions. This field is updated by this function.
    void GetVarsAndBoundsInPtrDeref(ElevatedCFGBlock *EB,
                                    BoundsMapTy &VarsAndBounds);

    // Get the mapping of variables to their resetted bounds because of a
    // modification to a variable that occurs in their lower or upper bounds
    // expressions.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] CurrStmt is the current statement.
    // @param[out] VarsAndBounds is a map of variables to their bounds
    // expressions. This field is updated by this function.
    void GetVarsAndBoundsForModifiedVars(ElevatedCFGBlock *EB,
                                         const Stmt *CurrStmt,
                                         BoundsMapTy &VarsAndBounds);

    // Check if CurrStmt is invertible w.r.t. the variables modified by
    // CurrStmt.
    // Note: This function modifies the set EB->InvertibleStmts.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] CurrStmt is the current statement.
    // @param[in] PtrsWithAffectedBounds is the set of variables that are
    // pointers to null-terminated arrays whose bounds are affected by
    // modification to variables that occur in their bounds expressions by
    // CurrStmt.
    void CheckStmtInvertibility(ElevatedCFGBlock *EB,
                                const Stmt *CurrStmt,
                                VarSetTy PtrsWithAffectedBounds) const;

    // Update the bounds in StmtOut with the adjusted bounds for the current
    // statement, if they exist.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] CurrStmt is the current statement.
    // @param[out] StmtOut is updated with the adjusted bounds for CurrStmt, if
    // they exist.
    void UpdateAdjustedBounds(ElevatedCFGBlock *EB, const Stmt *CurrStmt,
                              BoundsMapTy &StmtOut) const;

    // Order the blocks by block number to get a deterministic iteration order
    // for the blocks.
    // @return Blocks ordered by block number from higher to lower since block
    // numbers decrease from entry to exit.
    OrderedBlocksTy GetOrderedBlocks() const;

  }; // end of BoundsWideningAnalysis class.

} // end namespace clang
#endif
