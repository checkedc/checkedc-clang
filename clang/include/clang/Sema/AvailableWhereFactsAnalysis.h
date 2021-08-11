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

#include "clang/AST/CanonBounds.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Sema/CheckedCAnalysesPrepass.h"
#include "clang/Sema/Sema.h"

namespace clang {

  // StmtFactsMapTy denotes a map from a statement to the facts associated 
  // with it.
  using StmtFactsMapTy = llvm::DenseMap<const Stmt *, AbstractFactListTy>;

  // StmtSetTy denotes a set of statements.
  using StmtSetTy = llvm::SmallPtrSet<const Stmt *, 16>;

  // StmtMapTy denotes a map of a statement to another statement. This is used
  // to store the mapping of a statement to its previous statement in a block.
  using StmtMapTy = llvm::DenseMap<const Stmt *, const Stmt *>;

  // OrderedBlocksTy denotes blocks ordered by block numbers. This is useful
  // for printing the blocks in a deterministic order.
  using OrderedBlocksTy = std::vector<const CFGBlock *>;

  // AvailableFactsKillKind denotes two kinds of kill variables.
  // KillExpr denotes a variable to kill a EqualityOpFact or a InferredFact
  // KillBounds denotes a variable to kill a BoundsDeclFact
  enum AvailableFactsKillKind {
    KillExpr,
    KillBounds
  };

  // KillVar (a pair of VarDecl with its kind) and the containers based on it
  using KillVar = std::pair<VarDecl *, AvailableFactsKillKind>;
  using KillVarSetTy = llvm::SmallSet<KillVar, 2>;
  using StmtKillVarSetTy = llvm::DenseMap<const Stmt *, KillVarSetTy>;

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

  public:
    AvailableFactsUtil(Sema &SemaRef, CFG *Cfg,
                       ASTContext &Ctx, Lexicographic Lex) :
      SemaRef(SemaRef), Cfg(Cfg), Ctx(Ctx), Lex(Lex), OS(llvm::outs()) {}

    // Pretty print a expr
    void Print(const Expr *) const;

    // Pretty print a Stmt
    void Print(const Stmt *) const;

    // Pretty print an abstract fact
    void DumpAbstractFact(const AbstractFact *Fact) const;

    // Pretty print a list of abstract facts
    void DumpAbstractFacts(const AbstractFactListTy &Facts) const;

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

    // Determine the boolean state of an edge when the 
    // @param[in] PredBlock is a predecessor block of the current block.
    // @param[in] CurrBlock is the current block.
    // @return Returns true if true if on the edge, false otherwise.
    bool ConditionOnEdge(const CFGBlock *PredBlock,
                         const CFGBlock *CurrBlock) const;

    // Get all variables modified by CurrStmt or statements nested in CurrStmt.
    // @param[in] CurrStmt is a given statement.
    // @param[out] ModifiedVars is a set of variables modified by CurrStmt or
    // statements nested in CurrStmt.
    void GetModifiedVars(const Stmt *CurrStmt, VarSetTy &ModifiedVars) const;

    // Invoke IgnoreValuePreservingOperations to strip off casts.
    // @param[in] E is the expression whose casts must be stripped.
    // @return E with casts stripped off.
    Expr *IgnoreCasts(const Expr *E) const;

    // Based on IgnoreCasts, strip off more casts including IntegralCast and 
    // LValueToRValue
    Expr *TranspareCasts(const Expr *E) const;

    // We do not want to run dataflow analysis on null blocks or the exit
    // block. So we skip them.
    // @param[in] B is the block which may need to be skipped from dataflow
    // analysis.
    // @return Whether B should be skipped.
    bool SkipBlock(const CFGBlock *B) const;

    // Determine if a variable is used in a fact.
    bool IsVarInFact(const AbstractFact *Fact, const VarDecl *Var) const;

    // Determine if two facts equal.
    bool IsFactEqual(const AbstractFact *Fact1, const AbstractFact *Fact2) const;

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

  }; // end of AvailableFactsUtil class.

  // Note: Template specializations of a class member must be present at the
  // same namespace level as the class. So we need to declare template
  // specializations outside the class declaration.
  template<>
  AbstractFactListTy AvailableFactsUtil::Difference<AbstractFactListTy, KillVarSetTy>(
    AbstractFactListTy &A, KillVarSetTy &B) const;

  // Template specialization for computing the union of AbstractFactListTy.
  template<>
  AbstractFactListTy AvailableFactsUtil::Union<AbstractFactListTy>(
    AbstractFactListTy &A, AbstractFactListTy &B) const;

  // Template specialization for computing the intersection of AbstractFactListTy.
  template<>
  AbstractFactListTy AvailableFactsUtil::Intersect<AbstractFactListTy>(
    AbstractFactListTy &A, AbstractFactListTy &B) const;

  // Template specialization for determining the equality of AbstractFactListTy.
  template<>
  bool AvailableFactsUtil::IsEqual<AbstractFactListTy>(
    AbstractFactListTy &A, AbstractFactListTy &B) const;

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
    const bool DebugAvailableFacts;

    class ElevatedCFGBlock {
    public:
      using EdgeFactsTy = llvm::DenseMap<const ElevatedCFGBlock *, AbstractFactListTy>;

      const CFGBlock *Block;

      // Block-wise
      AbstractFactListTy In;
      KillVarSetTy Kill;

      // Edge-wise
      EdgeFactsTy Gen, Out;
      // Edge-wise (but on a Block)
      AbstractFactListTy GenAllSucc, OutAllSucc;

      // Statement-wise
      StmtFactsMapTy StmtGen;
      StmtKillVarSetTy StmtKill;

      // A mapping from a statement to its previous statement in a block.
      StmtMapTy PrevStmtMap;
      // The last statement of the block. This is nullptr if the block is empty.
      const Stmt *LastStmt = nullptr;

      ElevatedCFGBlock(const CFGBlock *B) : Block(B) {}
      ~ElevatedCFGBlock();

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
  
  public:
    // Top is a special bounds expression that denotes the super set of all
    // bounds expressions.
    static constexpr RangeBoundsExpr *Top = nullptr;

    AvailableWhereFactsAnalysis(Sema &SemaRef, CFG *Cfg) :
      SemaRef(SemaRef), Cfg(Cfg), Ctx(SemaRef.Context),
      Lex(Lexicographic(Ctx, nullptr)), OS(llvm::outs()),
      AFUtil(AvailableFactsUtil(SemaRef, Cfg, Ctx, Lex)),
      DebugAvailableFacts(SemaRef.getLangOpts().DebugAvailableFacts) {}

    ~AvailableWhereFactsAnalysis();
    
    // Run the dataflow analysis.
    // @param[in] FD is the current function.
    void Analyze(FunctionDecl *FD, StmtSetTy NestedStmts);

    // Pretty print the widened bounds for all null-terminated arrays in the
    // current function.
    // @param[in] FD is the current function.
    void DumpAvailableFacts(FunctionDecl *FD);

  private:
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
                                StmtSetTy NestedStmts);

    void CollectFactsInDecl(AbstractFactListTy &Gen,
                            KillVarSetTy &Kill,
                            const VarDecl *V);

    // Collect the facts and killed varibles in the where clauses.
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

    // Get the Out set for the statement. 
    AbstractFactListTy GetStmtOut(ElevatedCFGBlock *EB, const Stmt *CurrStmt) const;

    // Get the In set for the statement. 
    AbstractFactListTy GetStmtIn(ElevatedCFGBlock *EB, const Stmt *CurrStmt) const;

    // Order the blocks by block number to get a deterministic iteration order
    // for the blocks.
    // @return Blocks ordered by block number from higher to lower since block
    // numbers decrease from entry to exit.
    OrderedBlocksTy GetOrderedBlocks() const;
  }; // end of AvailableWhereFactsAnalysis class.

} // end namespace clang

#endif
