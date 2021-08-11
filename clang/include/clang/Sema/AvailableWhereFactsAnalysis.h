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

  // OrderedBlocksTy denotes blocks ordered by block numbers. This is useful
  // for printing the blocks in a deterministic order.
  using OrderedBlocksTy = std::vector<const CFGBlock *>;

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
    void PrintVarSet(VarSetTy VarSet) const;

  }; // end of AvailableFactsUtil class.

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
    AvailableWhereFactsAnalysis(Sema &SemaRef, CFG *Cfg) :
      SemaRef(SemaRef), Cfg(Cfg), Ctx(SemaRef.Context),
      Lex(Lexicographic(Ctx, nullptr)), OS(llvm::outs()),
      AFUtil(AvailableFactsUtil(SemaRef, Cfg, Ctx, Lex)),
      DebugAvailableFacts(SemaRef.getLangOpts().DebugAvailableFacts) {}

    // Run the dataflow analysis.
    // @param[in] FD is the current function.
    void Analyze(FunctionDecl *FD, StmtSetTy NestedStmts);

    // Pretty print the widened bounds for all null-terminated arrays in the
    // current function.
    // @param[in] FD is the current function.
    void DumpAvailableFacts(FunctionDecl *FD);

  }; // end of AvailableWhereFactsAnalysis class.

} // end namespace clang

#endif
