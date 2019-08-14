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

  std::vector<Comparison> AllComparisons;
  std::queue<ElevatedCFGBlock *> WorkList;
  std::vector<ElevatedCFGBlock *> Blocks;

  PostOrderCFGView POView = PostOrderCFGView(Cfg);
  unsigned int MaxBlockID = 0;
  std::vector<unsigned int> InWorkList;
  for (const CFGBlock *Block : POView)
    if (Block->getBlockID() > MaxBlockID)
      MaxBlockID = Block->getBlockID();

   BlockIDs.clear();
   BlockIDs.resize(MaxBlockID + 1, 0);
   for (const CFGBlock *Block : POView) {
     auto NewBlock = new ElevatedCFGBlock(Block);
     WorkList.push(NewBlock);
     InWorkList.push_back(Block->getBlockID());
     Blocks.push_back(NewBlock);
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
     AllComparisons.insert(AllComparisons.end(), B->GenThen.begin(), B->GenThen.end());
     AllComparisons.insert(AllComparisons.end(), B->GenElse.begin(), B->GenElse.end());
   }

   // Which comparisons contain pointer derefs?
   std::vector<bool> ComparisonContainsDeref;
   for (auto C : AllComparisons) {
     if (ContainsPointerDeref(C))
       ComparisonContainsDeref.push_back(true);
     else
       ComparisonContainsDeref.push_back(false);
   }

   // Which blocks contain potential pointer assignments? (assignment via a pointer or a call)
   std::vector<bool> PointerAssignmentInBlocks(Blocks.size(), false);
   for (unsigned int Index = 0; Index < Blocks.size(); Index++) {
     std::set<const VarDecl *> DefinedVars;
     for (CFGElement Elem : *(Blocks[Index]->Block))
       if (Elem.getKind() == CFGElement::Statement)
         CollectDefinedVars(Elem.castAs<CFGStmt>().getStmt(), DefinedVars);

     if (std::any_of(DefinedVars.begin(), DefinedVars.end(),
                     [](const VarDecl *VD){return VD->getType()->isPointerType();}))
       PointerAssignmentInBlocks[Index] = true;
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
   // If an expression in a comparison contains a pointer deref, kill the comparison
   // at any potential pointer assignment expression.
   // A pointer deref can appear in any of the following forms:
   // *p, ->, (*p)., *(p+1), or p[1]
   for (int CompInd = 0; CompInd < AllComparisons.size(); CompInd++)
     if (ComparisonContainsDeref[CompInd])
       for (int BlockInd = 0; BlockInd < Blocks.size(); BlockInd++)
         if (PointerAssignmentInBlocks[BlockInd])
           Blocks[BlockInd]->Kill.insert(AllComparisons[CompInd]);


   // Iterative Worklist Algorithm
   while (!WorkList.empty()) {
     ElevatedCFGBlock *CurrentBlock = WorkList.front();
     InWorkList.erase(std::remove(InWorkList.begin(),
                                     InWorkList.end(),
                                     CurrentBlock->Block->getBlockID()),
                                     InWorkList.end());
     WorkList.pop();

     // Update In set
     ComparisonSet Intersecions;
     bool FirstIteration = true;
     for (auto I : CurrentBlock->Block->preds()) {
       if (!I)
         continue;
       if (I->succ_size() == 2) {
         if (*(I->succ_begin()) == CurrentBlock->Block) {
           if (FirstIteration) {
             Intersecions = Blocks[BlockIDs[I->getBlockID()]]->OutThen;
             FirstIteration = false;
           } else
             Intersecions = Intersect(Intersecions, Blocks[BlockIDs[I->getBlockID()]]->OutThen);
         } else {
           if (FirstIteration) {
             Intersecions = Blocks[BlockIDs[I->getBlockID()]]->OutElse;
             FirstIteration = false;
           } else
             Intersecions = Intersect(Intersecions, Blocks[BlockIDs[I->getBlockID()]]->OutElse);
         }
       } else if (I->succ_size() == 1) {
         if (FirstIteration) {
           Intersecions = Blocks[BlockIDs[I->getBlockID()]]->OutThen;
           FirstIteration = false;
         } else
           Intersecions = Intersect(Intersecions, Blocks[BlockIDs[I->getBlockID()]]->OutThen);
       }
     }
     CurrentBlock->In = Intersecions;

     // Update Out Set
     ComparisonSet OldOutThen = CurrentBlock->OutThen;
     ComparisonSet OldOutElse = CurrentBlock->OutElse;
     ComparisonSet UnionThen = Union(CurrentBlock->In, CurrentBlock->GenThen);
     ComparisonSet UnionElse = Union(CurrentBlock->In, CurrentBlock->GenElse);
     CurrentBlock->OutThen = Difference(UnionThen, CurrentBlock->Kill);
     CurrentBlock->OutElse = Difference(UnionElse, CurrentBlock->Kill);

     // Recompute the Affected Blocks and _uniquely_ add them to the worklist
     if (Differ(OldOutThen, CurrentBlock->OutThen) ||
         Differ(OldOutElse, CurrentBlock->OutElse))
       for (auto I : CurrentBlock->Block->succs()) {
         if (std::find(InWorkList.begin(), InWorkList.end(), I->getBlockID()) ==
             InWorkList.end()) {
           InWorkList.push_back(I->getBlockID());
           WorkList.push(Blocks[BlockIDs[I->getBlockID()]]);
         }
       }
   }
   for (auto B : Blocks)
     Facts.push_back(std::pair<ComparisonSet, ComparisonSet>(B->In, B->Kill));

   while(!Blocks.empty()) {
     delete Blocks.back();
     Blocks.pop_back();
   }
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
void AvailableFactsAnalysis::GetFacts(std::pair<ComparisonSet, ComparisonSet> &CFacts) {
  CFacts = Facts[CurrentIndex];
}

// Given two sets S1 and S2, the return value is S1 - S2.
ComparisonSet AvailableFactsAnalysis::Difference(ComparisonSet &S1, ComparisonSet &S2) {
 if (S2.size() == 0)
    return S1;
  ComparisonSet Result;
  for (auto E1 : S1)
    if (S2.find(E1) == S2.end())
      Result.insert(E1);
  return Result;
}

// Given two sets S1 and S2, the return value is the union of these sets.
ComparisonSet AvailableFactsAnalysis::Union(ComparisonSet &S1, ComparisonSet &S2) {
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
bool AvailableFactsAnalysis::Differ(ComparisonSet &S1, ComparisonSet &S2) {
  if (S1.size() != S2.size())
    return true;
  for (auto E : S1)
    if (S2.find(E) == S2.end())
      return true;
  return false;
}

// Given two sets S1 and S2, the return value is the intersection of these sets.
ComparisonSet AvailableFactsAnalysis::Intersect(ComparisonSet &S1, ComparisonSet &S2) {
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
  std::set<const Expr *> Exprs;
  CollectExpressions(I.first, Exprs);
  CollectExpressions(I.second, Exprs);
  for (auto InnerExpr : Exprs)
    if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(InnerExpr))
      if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl()))
        if (VD == V)
          return true;
  return false;
}

bool AvailableFactsAnalysis::ContainsPointerDeref(Comparison& I) {
  std::set<const Expr *> Exprs;
  CollectExpressions(I.first, Exprs);
  CollectExpressions(I.second, Exprs);
  for (auto InnerExpr : Exprs) {
    if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(InnerExpr))
      if (UO->getOpcode() == UO_Deref)
        return true;
    if (const MemberExpr *ME = dyn_cast<MemberExpr>(InnerExpr))
      if (ME->isArrow())
        return true;
    if (const ArraySubscriptExpr *A = dyn_cast<ArraySubscriptExpr>(InnerExpr))
      return true;
  }
  return false;
}

// This function return true if an expression is volatile, and false otherwise.
// An expression is volatile if there is at least one volatile variable in it.
bool AvailableFactsAnalysis::IsVolatile(const Expr *E) {
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E))
    if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl()))
      if (VD->getType().isVolatileQualified())
        return true;
  for (auto Child : E->children()) {
    if (const Expr *EChild = dyn_cast<Expr>(Child))
      if (IsVolatile(EChild))
        return true;
  }
  return false;
}

bool AvailableFactsAnalysis::ContainsFunctionCall(const Expr *E) {
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E))
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(DRE->getDecl()))
      return true;
  for (auto Child : E->children()) {
    if (const Expr *EChild = dyn_cast<Expr>(Child))
      if (ContainsFunctionCall(EChild))
        return true;
  }
  return false;
}

// This function computes a list of comparisons E1 <= E2 from `E`.
// - If `E` is a simple direct comparison expression `A op B`, then the comparison
// can be created if `op` is one of LE, LT, GE, GT, or EQ.
// - If `E` has the form `A && B`, comparisons can be created for A and B.
// Note that we do not include comparisons whose expressions involve
// calls or references to volatile variables.
//
// Some examples:
// - `E` has the form A < B: add (A, B) to `ISet`.
// - `E` has the form `E1 && E2`: ExtractComparisons in E1 and E2 and add them to `ISet`.
// TODO: handle the case where logical negation operator (!) is used.
void AvailableFactsAnalysis::ExtractComparisons(const Expr *E, ComparisonSet &ISet) {
  if (IsVolatile(E) || ContainsFunctionCall(E))
    return;
  if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens())) {
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
}

// This function computes a list of negated comparisons from `E`.
// - If `E` is a simple direct comparison expression `A op B`, then the negated
//   comparison can be created if `op` is one of LE, LT, GE, GT, or NE.
// - If `E` has the form `A || B`, negated comparisons can be created for A and B.
// Note that we do not include comparisons whose expressions involve
// calls or references to volatile variables.
//
// Some examples:
// - `E` has the form A < B: add (B, A) to `ISet`.
// - `E` has the form `E1 || E2`: ExtractNegatedComparisons in E1 and E2 and add
//   them to `ISet`.
void AvailableFactsAnalysis::ExtractNegatedComparisons(const Expr *E, ComparisonSet &ISet) {
  if (IsVolatile(E) || ContainsFunctionCall(E))
    return;
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

void AvailableFactsAnalysis::CollectExpressions(const Stmt *St, std::set<const Expr *> &AllExprs) {
  if (!St)
    return;
  if (const Expr *E = dyn_cast<Expr>(St))
    AllExprs.insert(E);
  for (auto I = St->child_begin(); I != St->child_end(); ++I)
    CollectExpressions(*I, AllExprs);
}

// This function collects the defined variables in statement `St`.
// We assume a variable is define:
// 1. if it appears in the left hand side of an assignment
// 2. if it is a pointer which is passed as an argument of a function call
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
  } else if (const DeclRefExpr *DRE = dyn_cast<const DeclRefExpr>(St)) {
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(DRE->getDecl()))
      for (auto V : FD->parameters())
        if (V->getType()->isPointerType())
          DefinedVars.insert(V);
  }

  for (auto I : St->children())
    CollectDefinedVars(I, DefinedVars);
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
  Reset();
  for (unsigned int Index = 0; Index < BlockIDs.size(); Index++) {
    OS << "// CHECK-NEXT: Block #" << (std::find(BlockIDs.begin(), BlockIDs.end(), Index) - BlockIDs.begin()) << ": {\n";
    std::pair<ComparisonSet, ComparisonSet> Facts;
    GetFacts(Facts);
    PrintComparisonSet(OS, Facts.first, "// CHECK-NEXT: In");
    PrintComparisonSet(OS, Facts.second, "// CHECK-NEXT: Kill");
    OS << "// CHECK-NEXT: }\n";
    Next();
  }
  Reset();
  llvm::outs() << "===============================\n";
}
}

