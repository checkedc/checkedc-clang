//===-------- AvailableFactsAnalysis.cpp - collect comparison facts -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements an analysis for collecting comparison facts.
//
//  The analysis has the following characteristics: 1. forward dataflow analysis,
//  2. conservative, 3. intra-procedural, and 4. path-sensitive.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/AvailableFactsAnalysis.h"

namespace clang {
class Sema;

void AvailableFactsAnalysis::Analyze() {
  assert(Cfg && "expected CFG to exist");
  ComparisonSet AllComparisons;

  PostOrderCFGView POView = PostOrderCFGView(Cfg);
  unsigned int MaxBlockID = 0;
  for (const CFGBlock *Block : POView)
    if (Block->getBlockID() > MaxBlockID)
      MaxBlockID = Block->getBlockID();

   BlockIDs.resize(MaxBlockID + 1, 0);
   for (const CFGBlock *Block : POView) {
     auto NewBlock = new ElevatedCFGBlock(Block);
     WorkList.push(NewBlock);
     Blocks.emplace_back(NewBlock);
     BlockIDs[Block->getBlockID()] = Blocks.size() - 1;
   }

   // Compute Gen Sets
   for (auto B : Blocks) {
     if (const Stmt *Term = B->Block->getTerminator()) {
       if(const IfStmt *IS = dyn_cast<IfStmt>(Term)) {
         ComparisonSet Comparisons;
         ExtractComparisons(IS->getCond(), Comparisons);
         B->GenThen.insert(Comparisons.begin(), Comparisons.end());

         ComparisonSet NegatedComparisons;
         ExtractNegatedComparisons(IS->getCond(), NegatedComparisons);
         B->GenElse.insert(NegatedComparisons.begin(), NegatedComparisons.end());
       }
     }
     AllComparisons.insert(B->GenThen.begin(), B->GenThen.end());
     AllComparisons.insert(B->GenElse.begin(), B->GenElse.end());
   }

   // Compute Kill Sets
   for (auto B : Blocks) {
     std::set<const VarDecl *> DefinedVars;
     for (CFGElement Elem : *(B->Block))
       if (Elem.getKind() == CFGElement::Statement)
         CollectDefinedVars(Elem.castAs<CFGStmt>().getStmt(), DefinedVars);

     for (auto E : AllComparisons)
       for (auto V : DefinedVars)
         if (ContainsVariable(E, V))
           B->Kill.insert(E);
   }

   // Iterative Worklist Algorithm
   while (!WorkList.empty()) {
     ElevatedCFGBlock *CurrentBlock = WorkList.front();
     WorkList.pop();

     // Update In set
     ComparisonSet IntermediateIntersecions;
     bool FirstIteration = true;
     for (CFGBlock::const_pred_iterator I = CurrentBlock->Block->pred_begin(), E = CurrentBlock->Block->pred_end(); I != E; ++I) {
       if (!*I)
         continue;
       if (*((*I)->succ_begin()) == CurrentBlock->Block) {
         if (FirstIteration) {
           IntermediateIntersecions = GetByCFGBlock(*I)->OutThen;
           FirstIteration = false;
         } else
           IntermediateIntersecions = Intersect(IntermediateIntersecions, CurrentBlock->OutThen);
       } else {
         if (FirstIteration) {
           IntermediateIntersecions = GetByCFGBlock(*I)->OutElse;
           FirstIteration = false;
         } else
           IntermediateIntersecions = Intersect(IntermediateIntersecions, CurrentBlock->OutElse);
       }
     }
     CurrentBlock->In = IntermediateIntersecions;

     // Update Out Set
     ComparisonSet OldOutThen = CurrentBlock->OutThen, OldOutElse = CurrentBlock->OutElse;
     ComparisonSet UnionThen = Union(CurrentBlock->In, CurrentBlock->GenThen);
     ComparisonSet UnionElse = Union(CurrentBlock->In, CurrentBlock->GenElse);
     CurrentBlock->OutThen = Difference(UnionThen, CurrentBlock->Kill);
     CurrentBlock->OutElse = Difference(UnionElse, CurrentBlock->Kill);

     // Recompute the Affected Blocks
     if (Differ(OldOutThen, CurrentBlock->OutThen) || Differ(OldOutElse, CurrentBlock->OutElse))
       for (CFGBlock::const_succ_iterator I = CurrentBlock->Block->succ_begin(), E = CurrentBlock->Block->succ_end(); I != E; ++I)
         WorkList.push(GetByCFGBlock(*I));
   }

   if (DumpFacts)
     DumpComparisonFacts(llvm::outs());
}

void AvailableFactsAnalysis::Reset() {
  CurrentIndex = 0;
}

void AvailableFactsAnalysis::Next() {
  CurrentIndex++;
}

// This function fills `ComparisonFacts` with pairs (Expr1, Expr2) where
// Expr1 <= Expr2.
// These comparisons correspond to the current block.
void AvailableFactsAnalysis::GetFacts(std::set<std::pair<Expr *, Expr *>>& ComparisonFacts) {
  ComparisonFacts = Blocks[CurrentIndex]->In;
}

AvailableFactsAnalysis::ElevatedCFGBlock* AvailableFactsAnalysis::GetByCFGBlock(const CFGBlock *B) {
  return Blocks[BlockIDs[B->getBlockID()]];
}

// Given two sets S1 and S2, the return value is S1 \ S2.
ComparisonSet AvailableFactsAnalysis::Difference(ComparisonSet& S1, ComparisonSet& S2) {
 if (S2.size() == 0)
    return S1;
  ComparisonSet Result;
  for (auto E1 : S1)
    if (S2.find(E1) == S2.end())
      Result.insert(E1);
  return Result;
}

// Given two sets S1 and S2, the return value is the union of these sets.
ComparisonSet AvailableFactsAnalysis::Union(ComparisonSet& S1, ComparisonSet& S2) {
  if (S1.size() == 0)
    return S2;
  if (S2.size() == 0)
    return S1;
  ComparisonSet Result(S1);
  Result.insert(S2.begin(), S2.end());
  return Result;
}

// Given two sets S1 and S2, this function returns true if the two sets
// are not equal.
// Equality means that their sizes are the same, and every member of S1 is found in
// S2 and vice versa.
bool AvailableFactsAnalysis::Differ(ComparisonSet& S1, ComparisonSet& S2) {
  if (S1.size() != S2.size())
    return true;
  for (auto E : S1)
    if (S2.find(E) == S2.end())
      return true;
  return false;
}

// Given two sets S1 and S2, the return value is the intersection of these sets.
ComparisonSet AvailableFactsAnalysis::Intersect(ComparisonSet& S1, ComparisonSet& S2) {
  if (S1.size() == 0)
    return S1;
  if (S2.size() == 0)
    return S2;
  ComparisonSet Result;

  for (auto E1 : S1)
    if (S2.find(E1) != S2.end())
      Result.insert(E1);
  return Result;
}

// This function returns true if variable V is used in the comparison I.
bool AvailableFactsAnalysis::ContainsVariable(Comparison& I, const VarDecl *V) {
  ExprSet Exprs;
  CollectExpressions(I.first, Exprs);
  CollectExpressions(I.second, Exprs);
  for (auto InnerExpr : Exprs)
    if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(InnerExpr))
      if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl()))
        if (VD == V)
          return true;
  return false;
}

// This function computes a list of comparisons E1 <= E2 from `E`.
// - If `E` is a simple direct comparison expression `A op B`, then the comparison
// can be created if `op` is one of LE, LT, GE, GT, or EQ.
// - If `E` has the form `A && B`, comparisons can be created for A and B.
//
// Some examples:
// - `E` has the form A < B: add (A, B) to `ISet`.
// - `E` has the form `E1 && E2`: ExtractComparisons in E1 and E2 and add them to `ISet`.
// TODO: handle the case where logical negation operator (!) is used.
void AvailableFactsAnalysis::ExtractComparisons(const Expr *E, ComparisonSet &ISet) {
  if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens()))
    switch (BO->getOpcode()) {
      case BinaryOperatorKind::BO_LE:
      case BinaryOperatorKind::BO_LT:
        ISet.insert(Comparison(BO->getLHS(), BO->getRHS()));
        break;
      case BinaryOperatorKind::BO_GE:
      case BinaryOperatorKind::BO_GT:
        ISet.insert(Comparison(BO->getRHS(), BO->getLHS()));
        break;
      case BinaryOperatorKind::BO_EQ:
        ISet.insert(Comparison(BO->getRHS(), BO->getLHS()));
        ISet.insert(Comparison(BO->getLHS(), BO->getRHS()));
        break;
      case BinaryOperatorKind::BO_LAnd:
        ExtractComparisons(BO->getRHS(), ISet);
        ExtractComparisons(BO->getLHS(), ISet);
        break;
      default:
        break;
    }
}

// This function computes a list of negated comparisons from `E`.
// - If `E` is a simple direct comparison expression `A op B`, then the negated
//   comparison can be created if `op` is one of LE, LT, GE, GT, or NE.
// - If `E` has the form `A || B`, negated comparisons can be created for A and B.
// Some examples:
// - `E` has the form A < B: add (B, A) to `ISet`.
// - `E` has the form `E1 || E2`: ExtractNegatedComparisons in E1 and E2 and add them to `ISet`.
void AvailableFactsAnalysis::ExtractNegatedComparisons(const Expr *E, ComparisonSet &ISet) {
  if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens()))
    switch (BO->getOpcode()) {
      case BinaryOperatorKind::BO_LE:
      case BinaryOperatorKind::BO_LT:
        ISet.insert(Comparison(BO->getRHS(), BO->getLHS()));
        break;
      case BinaryOperatorKind::BO_GE:
      case BinaryOperatorKind::BO_GT:
        ISet.insert(Comparison(BO->getLHS(), BO->getRHS()));
        break;
      case BinaryOperatorKind::BO_NE:
        ISet.insert(Comparison(BO->getRHS(), BO->getLHS()));
        ISet.insert(Comparison(BO->getLHS(), BO->getRHS()));
        break;
      case BinaryOperatorKind::BO_LOr:
        ExtractNegatedComparisons(BO->getRHS(), ISet);
        ExtractNegatedComparisons(BO->getLHS(), ISet);
        break;
      default:
        break;
    }
}

void AvailableFactsAnalysis::CollectExpressions(const Stmt *St, ExprSet &AllExprs) {
  if (!St)
    return;
  if (const Expr *E = dyn_cast<Expr>(St))
    AllExprs.insert(E);
  for (auto I = St->child_begin(); I != St->child_end(); ++I)
    CollectExpressions(*I, AllExprs);
}

void AvailableFactsAnalysis::CollectDefinedVars(const Stmt *St, std::set<const VarDecl *> &DefinedVars) {
  if (!St)
    return;

  if (const BinaryOperator *BO = dyn_cast<const BinaryOperator>(St)) {
    if (BO->isAssignmentOp()) {
      Expr *LHS = BO->getLHS()->ignoreParenBaseCasts()->IgnoreImpCasts();
      if (const DeclRefExpr *D = dyn_cast<const DeclRefExpr>(LHS))
        if (const VarDecl *V = dyn_cast<const VarDecl>(D->getDecl()))
          DefinedVars.insert(V);
    }
  } else if (const UnaryOperator *UO = dyn_cast<const UnaryOperator>(St)) {
    if (UO->isIncrementDecrementOp()) {
      Expr *LHS = UO->getSubExpr()->ignoreParenBaseCasts()->IgnoreImpCasts();
      if (const DeclRefExpr *D = dyn_cast<const DeclRefExpr>(LHS))
        if (const VarDecl *V = dyn_cast<const VarDecl>(D->getDecl()))
          DefinedVars.insert(V);
    }
  }

  for (auto I = St->child_begin(); I != St->child_end(); ++I)
    CollectDefinedVars(*I, DefinedVars);
}

void AvailableFactsAnalysis::PrintComparisonSet(raw_ostream &OS, ComparisonSet &ISet, std::string Title) {
  OS << Title << ": ";
  for (auto I : ISet) {
    OS << "(";
    I.first->printPretty(OS, nullptr, PrintingPolicy(S.Context.getLangOpts()));
    OS << ", ";
    I.second->printPretty(OS, nullptr, PrintingPolicy(S.Context.getLangOpts()));
    OS << "), ";
  }
  OS << "\n";
}

void AvailableFactsAnalysis::DumpComparisonFacts(raw_ostream &OS) {
  for (auto B : Blocks) {
    OS << "Block #" << B->Block->getBlockID() << ": {\n";
    PrintComparisonSet(OS, B->In, "In");
    PrintComparisonSet(OS, B->Kill, "Kill");
    OS << "}\n";
  }
}
}