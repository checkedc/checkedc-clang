//===== CheckedCSMTProver.h - Manager class for available facts ====//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
//  This file defines a SMT prover for CheckedC.
//===---------------------------------------------------------------------===//

#ifndef LLVM_CHECKEDC_SMT_PROVER_H
#define LLVM_CHECKEDC_SMT_PROVER_H

#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/AST/CanonBounds.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Sema/CheckedCAnalysesPrepass.h"
#include "clang/Sema/Sema.h"

// TODO: use project custom flag to just enable Z3 for this checking, not the whole llvm
#if LLVM_WITH_Z3
#include "llvm/Support/SMTAPI.h"
#endif

namespace clang {
  // ExprSetTy denotes a set of expressions and is used for an entry in EquivExprs.
  using ExprSetTy = llvm::SmallVector<Expr *, 4>;

  // The four MapTy is for the map for cached mappings.
  // Notes `llvm::SMTExprRef` is `const llvm::SMTExprRef *`.
  using ExprSymMapTy = llvm::DenseMap<const Expr *, llvm::SMTExprRef>;
  using FactSymMapTy = llvm::DenseMap<const AbstractFact *, llvm::SMTExprRef>;
  using VarDeclSymMapTy = llvm::DenseMap<const VarDecl *, llvm::SMTExprRef>;
  using SymVarDeclMapTy = llvm::DenseMap<llvm::SMTExprRef, const VarDecl *>;
}

namespace clang {
  class SMTExprBuilder;

  class CheckedCSMTProver {
  public:
    // Refer to SemeBounds::CheckBoundsDeclarations::ProofResult
    enum class ProofResult {
      True,  // Definitely provable.
      False, // Definitely false (an error)
      Maybe  // We're not sure yet.
    };

  private:
    Sema &S;
    CFG *Cfg;
    ASTContext &Ctx;
    llvm::raw_ostream &OS;

    llvm::SMTSolverRef Solver;

    // The maps for cached mappings.
    ExprSymMapTy ExprSymMap;
    FactSymMapTy FactSymMap;
    VarDeclSymMapTy VarDeclSymMap;
    SymVarDeclMapTy SymVarDeclMap;

    // SMTExprBuilder is the post-order AST visitor to construct the SMT constraint
    // expression a.k.a. SMTExprRef from the AST.
    // A SMTExprBuilder can be constucted many times but the mapping from an expression,
    // a fact, a variable declaraion is shared across the prover.
    friend class SMTExprBuilder;
  public:
    CheckedCSMTProver(Sema &S, CFG *Cfg) :
      S(S), Cfg(Cfg), Ctx(S.Context), OS(llvm::outs()),
      // TODO: config timeout of each query
      // TODO: check the meaning of timeout
      Solver(llvm::CreateZ3Solver()) {}
    ~CheckedCSMTProver();

    // TODO: enable timeout e.g. SetOption(SMT.timeout)
    //
    // TODO: 
    //   Sema::ProveBoundsDeclValidity contains just bounds, so does ProveBounds.
    //   There is no way to normalize the bounds without the variable.
    //   Besides, with the variable `x` we can also refine the constraints on bounds.
    //
    // Prove the source bounds can imply the declared bounds, given the equivalent variables
    // and the available facts just before the proof point.
    // The proof is divided into six steps which are the following six private methods.
    // @param[in] DeclaredBounds is the declared bounds.
    // @param[in] SrcBounds is the source bounds.
    // @param[in] EquivExprs is the set list of equivalent variables.
    // @param[in] Facts is the available facts.
    // @return Returns the prove result.
    ProofResult ProveBounds(const BoundsExpr *DeclaredBounds,
                     const BoundsExpr *SrcBounds,
                     EquivExprSets *EquivExprs,
                     AbstractFactListTy &Facts);

  private:
    // Step 1, check the validity of the bounds range
    bool GetValidRangeBounds(const BoundsExpr *DeclaredBounds,
                             const BoundsExpr *SrcBounds,
                             const RangeBoundsExpr **DeclaredRBE,
                             const RangeBoundsExpr **SrcRBE);

    // Step 2, build the constraints of bounds safety.
    llvm::SMTExprRef BuildSafeBoundsConstraints(
                                    const RangeBoundsExpr *DeclaredRBE, 
                                    const RangeBoundsExpr *SrcRBE);

    // Step 3, build the constraints of the equivalent expressions.
    llvm::SMTExprRef BuildEquivExprsConstraints(
                                    EquivExprSets *EquivExprs);

    // Step 4, build the constraints of the available facts at the proof point.
    llvm::SMTExprRef BuildAvailableFactConstraints(AbstractFactListTy &Facts);

    // Step 5, build the negation of the safety target.
    llvm::SMTExprRef BuildNegationOfTarget(llvm::SMTExprRef SymSafeBounds,
                                           llvm::SMTExprRef SymEquivExprs,
                                           llvm::SMTExprRef SymFacts);

    // Step 6, call the solver and handle the result.
    ProofResult CheckSafety(llvm::SMTExprRef UnsatTarget);

    // Each mapping (Fact->Sym, VarDecl->Sym, Expr->Sym) has two APIs: 
    // One is for the top-level, which is called outside the builder. 
    //   It hit, it direcly returns.
    //   If missed, it calls the builder. The builder will update the map.
    // The other is called inside the builder. 
    //   If hit, it early returns in the builder; 
    //   If missed, it continues the traversal and save the result afterwards.
    
    // Top-level, get or create-and-save.
    llvm::SMTExprRef FactToSym(AbstractFact *Fact);
    llvm::SMTExprRef VarDeclToSym(const VarDecl *VD);
    static std::string GetSymName(Sema &S, const VarDecl *VD);
    
    // Inside thd builder, hit or continue-and-save.
    llvm::SMTExprRef GetCachedSym(const VarDecl *VD);
    llvm::SMTExprRef GetCachedSym(const Expr *E);
    void SetCachedSym(const Expr *E, llvm::SMTExprRef Sym);
    // Set the bidiretional mapping between a varable declaration and a smt expression. 
    void SetVarDeclSymBimapping(const VarDecl *VD, llvm::SMTExprRef Sym);

  }; // end of CheckedCSMTProver class.

} // end namespace clang

#endif
