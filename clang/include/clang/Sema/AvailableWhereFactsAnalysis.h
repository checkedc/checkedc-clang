//===== AvailableWhereFactsAnalysis.h - Dataflow analysis for available facts ====//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
//  This file defines the interface for a dataflow analysis for available
//  facts on where-clauses and other statements.
//===---------------------------------------------------------------------===//

#ifndef LLVM_AVAILABLE_WHERE_FACTS_ANALYSIS_H
#define LLVM_AVAILABLE_WHERE_FACTS_ANALYSIS_H

#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/AST/CanonBounds.h"
#include "clang/Sema/CheckedCAnalysesPrepass.h"
#include "clang/Sema/Sema.h"

namespace clang {

  // StmtSetTy denotes a set of statements.
  using StmtSetTy = llvm::SmallPtrSet<const Stmt *, 16>;

  // StmtMapTy denotes a map of a statement to another statement. This is used
  // to store the mapping of a statement to its previous statement in a block.
  using StmtMapTy = llvm::DenseMap<const Stmt *, const Stmt *>;

  // OrderedBlocksTy denotes blocks ordered by block numbers. This is useful
  // for printing the blocks in a deterministic order.
  using OrderedBlocksTy = std::vector<const CFGBlock *>;

  // KillExpr denotes a variable that kills an EqualityOpFact or an InferredFact
  // KillBounds denotes a variable that kills a BoundsDeclFact.
  enum class AvailableFactsKillKind {
    KillExpr,
    KillBounds
  };

  // KillVar (a pair of VarDecl with its kind) and the containers based on it.
  using KillVar = std::pair<VarDecl *, AvailableFactsKillKind>;
  using KillVarSetTy = llvm::SmallSet<KillVar, 2>;
  using StmtKillVarSetTy = llvm::DenseMap<const Stmt *, KillVarSetTy>;

  // FactComparisonMapTy denotes the comparsion result of two facts.
  using FactComparison = std::pair<const AbstractFact *, const AbstractFact *>;
  using FactComparisonMapTy = llvm::DenseMap<FactComparison, bool>;

} // end namespace clang

namespace clang {

  //===-------------------------------------------------------------------===//
  // Class definition of the AvailableFactsUtil class. This class contains
  // helper methods that are used by the AvailableWhereFactsAnalysis class to
  // perform the dataflow analysis. The AvailableWhereFactsAnalysis class is defined
  // later in this file.
  //===-------------------------------------------------------------------===//

  class AvailableFactsUtil {
  private:
    Sema &SemaRef;
    CFG *Cfg;
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;
    FactComparisonMapTy FactComparisonMap;

  public:
    AvailableFactsUtil(Sema &SemaRef, CFG *Cfg,
                       ASTContext &Ctx, Lexicographic Lex) :
      SemaRef(SemaRef), Cfg(Cfg), Ctx(Ctx), Lex(Lex), OS(llvm::outs()),
      FactComparisonMap(FactComparisonMapTy()) {}

    // Pretty print a set of variables.
    void PrintKillVarSet(KillVarSetTy VarSet) const;

    // Determine if the edge from PredBlock to CurrBlock is a fallthrough.
    // @param[in] PredBlock is a predecessor block of the current block.
    // @param[in] CurrBlock is the current block.
    // @return Returns true if the edge is a fallthrough, false otherwise.
    bool IsFallthroughEdge(const CFGBlock *PredBlock,
                           const CFGBlock *CurrBlock) const;

    // Determine if the current block begins a case of a switch-case.
    // @param[in] PredBlock is the previous block.
    // @param[in] CurrBlock is the current block.
    // @return Returns true if the current block begins a case.
    bool IsSwitchCaseBlock(const CFGBlock *PredBlock,
                           const CFGBlock *CurrBlock) const;

    // Determine the boolean state of an edge when the previous block is an if-condition
    // @param[in] PredBlock is a predecessor block of the current block.
    // @param[in] CurrBlock is the current block.
    // @return Returns true if true if on the edge, false otherwise.
    bool ConditionOnEdge(const CFGBlock *PredBlock,
                         const CFGBlock *CurrBlock) const;

    // Get all variables modified by CurrStmt or statements nested in CurrStmt.
    // @param[in] CurrStmt is a given statement.
    // @param[out] ModifiedVars is a set of variables modified by CurrStmt or
    // statements nested in CurrStmt.
    void GetModifiedVars(const Stmt *CurrStmt, VarSetTy &ModifiedVars);

    // We do not want to run dataflow analysis on null blocks or the exit
    // block. So we skip them.
    // @param[in] B is the block which may need to be skipped from dataflow
    // analysis.
    // @return Whether B should be skipped.
    bool SkipBlock(const CFGBlock *B) const;

    // Determine if a variable is used in a fact.
    bool IsVarInFact(const AbstractFact *Fact, const VarDecl *Var) const;

    // Determine if two facts are equal. First check if the comparison is checked before,
    // otherwise, perform the real check.
    bool IsFactEqual(const AbstractFact *Fact1, const AbstractFact *Fact2);

    // Check if two facts equal.
    bool CheckFactEqual(const AbstractFact *Fact1, const AbstractFact *Fact2) const;

    // Compute the set difference of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The set difference of sets A and B.
    template<class T, class U>
    T Difference(T &A, U &B);

    // Compute the intersection of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The intersection of sets A and B.
    template<class T>
    T Intersect(T &A, T &B);

    // Compute the union of sets A and B.
    // @param[in] A is a set.
    // @param[in] B is a set.
    // @return The union of sets A and B.
    template<class T>
    T Union(T &A, T &B);

  }; // end of AvailableFactsUtil class.

  // Note: Template specializations of a class member must be present at the
  // same namespace level as the class. So we need to declare template
  // specializations outside the class declaration.
  template<>
  AbstractFactListTy AvailableFactsUtil::Difference<AbstractFactListTy, KillVarSetTy>(
    AbstractFactListTy &A, KillVarSetTy &B);

  // Template specialization for computing the union of AbstractFactListTy.
  template<>
  AbstractFactListTy AvailableFactsUtil::Union<AbstractFactListTy>(
    AbstractFactListTy &A, AbstractFactListTy &B);

  // Template specialization for computing the intersection of AbstractFactListTy.
  template<>
  AbstractFactListTy AvailableFactsUtil::Intersect<AbstractFactListTy>(
    AbstractFactListTy &A, AbstractFactListTy &B);

} // end namespace clang

namespace clang {
  //===-------------------------------------------------------------------===//
  // Implementation of the methods in the AvailableWhereFactsAnalysis class. 
  // This is the main class that implements the dataflow analysis for  for 
  // available facts on where-clauses and other statements. The sets In, Out, 
  // Gen and Kill that are used by the analysis are members of this class. 
  // The class uses helper methods from the AvailableFactsUtil class that are
  // defined later in this file.
  //===-------------------------------------------------------------------===//

  class AvailableWhereFactsAnalysis {
  private:
    Sema &SemaRef;
    CFG *Cfg;
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;
    AvailableFactsUtil AFUtil;
    
    class ElevatedCFGBlock {
    public:
      using EdgeFactsTy = llvm::DenseMap<const ElevatedCFGBlock *, AbstractFactListTy>;

      const CFGBlock *Block;

      // Block-wise.
      AbstractFactListTy In;
      KillVarSetTy Kill;

      // Edge-wise.
      EdgeFactsTy Gen, Out;
      // Edge-wise (stored at its starting block).
      AbstractFactListTy GenAllSucc, OutAllSucc;

      // A mapping from a statement to its previous statement in a block.
      StmtMapTy PrevStmtMap;
      // The last statement of the block. This is nullptr if the block is empty.
      const Stmt *LastStmt = nullptr;

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

    // Allocated facts in the analysis
    AbstractFactListTy FactsCreated;

    // Stateful accumulated fact sets in a block.
    // This is for GetStmtIn/GetStmtOut, which are called per statement in a block sequentially.
    AbstractFactListTy AccuGen;
    KillVarSetTy AccuKill;
    const CFGBlock *AccuBlock;
    ElevatedCFGBlock *AccuEB;

  public:
    // Top is a special bounds expression that denotes the super set of all
    // bounds expressions.
    static constexpr RangeBoundsExpr *Top = nullptr;

    AvailableWhereFactsAnalysis(Sema &SemaRef, CFG *Cfg) :
      SemaRef(SemaRef), Cfg(Cfg), Ctx(SemaRef.Context),
      Lex(Lexicographic(Ctx, nullptr)), OS(llvm::outs()),
      AFUtil(AvailableFactsUtil(SemaRef, Cfg, SemaRef.Context, Lex)) {}

    ~AvailableWhereFactsAnalysis();

    // Run the dataflow analysis.
    // @param[in] FD is the current function.
    void Analyze(FunctionDecl *FD, StmtSetTy NestedStmts);

    // Pretty print the widened bounds for all null-terminated arrays in the
    // current function.
    // @param[in] FD is the current function.
    void DumpAvailableFacts(FunctionDecl *FD);

    // Get the Out set for the statement. 
    AbstractFactListTy GetStmtOut(const CFGBlock *EB, const Stmt *CurrStmt);

    // Get the In set for the statement. 
    AbstractFactListTy GetStmtIn(const CFGBlock *EB, const Stmt *CurrStmt);

  private:
    // Add the successors of the current block to WorkList.
    // @param[in] CurrBlock is the current block.
    // @param[in] WorkList stores the blocks remaining to be processed for the
    // fixpoint computation.
    void AddSuccsToWorkList(const CFGBlock *CurrBlock, WorkListTy &WorkList);

    // Compute Gen and Kill sets for the entry block.
    // @param[in] FD is the FunctionDecl of this intra-procedure analysis.
    // @param[in] EB is the ElevatedCFGBlock for the enty block.
    void ComputeEntryGenKillSets(FunctionDecl *FD, ElevatedCFGBlock *EB);

    // Compute Gen and Kill sets for the block and statements in the block.
    // @param[in] EB is the current ElevatedCFGBlock.
    void ComputeGenKillSets(ElevatedCFGBlock *EB, StmtSetTy NestedStmts);

    // Compute the StmtGen and StmtKill sets for a statement in a block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[in] CurrStmt is the current statement.
    void ComputeStmtGenKillSets(ElevatedCFGBlock *EB, const Stmt *CurrStmt,
                                StmtSetTy NestedStmts,
                                AbstractFactListTy &Gen,
                                KillVarSetTy &Kill);

    // Collect the facts in the variable declaration.
    // @param[in] Gen is the container for the facts set.
    // @param[in] Gen is the container for the Kill set.
    // @param[in] V is the variable declaration to act.
    void CollectFactsInDecl(AbstractFactListTy &Gen,
                            KillVarSetTy &Kill,
                            const VarDecl *V);

    // Collect the facts in the where clauses.
    // @param[in] Gen is the container for the facts set.
    // @param[in] Gen is the container for the Kill set.
    // @param[in] WC is the where clause to act.
    void CollectFactsInWhereClause(AbstractFactListTy &Gen,
                                   KillVarSetTy &Kill,
                                   WhereClause *WC);

    // Collect the facts on the edges between blocks (EB and its successors)
    // @param[in] EB is the current ElevatedCFGBlock.
    void InitBlockGenOut(ElevatedCFGBlock *EB);

    // Compute the In set for the block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @return Return true if the In set of the block has changed, false
    // otherwise.
    bool ComputeInSet(ElevatedCFGBlock *EB);

    // Compute the Out set for the block.
    // @param[in] EB is the current ElevatedCFGBlock.
    // @param[out] EB is added to WorkList if the Out set of EB changes.
    // @return Return true if the OutAll set of the block has changed, false
    // otherwise.
    bool ComputeOutSet(ElevatedCFGBlock *EB, WorkListTy &WorkList);

    // Order the blocks by block number to get a deterministic iteration order
    // for the blocks.
    // @return Blocks ordered by block number from higher to lower since block
    // numbers decrease from entry to exit.
    OrderedBlocksTy GetOrderedBlocks() const;
  }; // end of AvailableWhereFactsAnalysis class.

} // end namespace clang

#endif
