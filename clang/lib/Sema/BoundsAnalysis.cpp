//===--------- BoundsAnalysis.cpp - Bounds Widening Analysis --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements a dataflow analysis for bounds widening.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/BoundsAnalysis.h"

namespace clang {
// Legend:
// n ==> intersection.
// E ==> belongs.
// u ==> union.

void BoundsAnalysis::WidenBounds() {
  assert(Cfg && "expected CFG to exist");

  WorkListTy WorkList;
  BlockMapTy BlockMap;

  // Add each block to WorkList and create a mapping from Block to
  // ElevatedCFGBlock.
  for (const auto *B : PostOrderCFGView(Cfg)) {
    // We do not want to process entry and exit blocks.
    if (SkipBlock(B))
      continue;
    auto EB = new ElevatedCFGBlock(B);
    WorkList.push(EB);
    BlockMap[B] = EB;
  }

  // Compute Gen and Kill sets.
  ComputeGenSets(BlockMap);
  ComputeKillSets(BlockMap);

  // Compute In and Out sets.
  while (!WorkList.empty()) {
    auto *EB = WorkList.front();
    WorkList.pop(EB);

    ComputeInSets(EB, BlockMap);
    ComputeOutSets(EB, BlockMap, WorkList);
  }

  CollectWidenedBounds(BlockMap);
}

void BoundsAnalysis::ComputeGenSets(BlockMapTy BlockMap) {
  // If there is an edge B1->B2 and the edge condition is of the form
  // "if (*(p + i))" then Gen[B1] = {B2, p:i+1} .

  for (const auto B : BlockMap) {
    auto EB = B.second;
    for (const CFGBlock *pred : EB->Block->preds()) {
      if (SkipBlock(pred))
        continue;

      // Check if the edge condition is of the form "if (*(p + i))".
      if (Expr *E = GetTerminatorCondition(pred)) {
        if (!ContainsPointerDeref(E))
          continue;

        // We can add "p:i+1" only on the true edge.
        // For example,
        // B1: if (*(p+ i))
        // B2:   foo();
        // B3: else bar();

        // Here we have the edges (B1->B2) and (B1->B3). We can add "p:i+1" only
        // on the true edge. Which means we will add the following entry to
        // Gen[B1]: {B2, p:i+1}
        if (const auto *I = pred->succs().begin())
          if (*I != EB->Block)
            continue;

        FillGenSet(E, BlockMap[pred], EB->Block);
      }
    }
  }
}

void BoundsAnalysis::FillGenSet(Expr *E, ElevatedCFGBlock *EB,
                                const CFGBlock *succ) {
  E = IgnoreCasts(E);

  if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
    const auto *Exp = IgnoreCasts(UO->getSubExpr());
    if (!Exp)
      return;

    // Note: When we have if conditions of the form
    // "if (*p && *(p+1) && *(p+2))" the effective bounds for p would be the
    // max of the computed bounds of all three expressions.

    // For conditions of the form "if (*p)".
    if (const auto *D = dyn_cast<DeclRefExpr>(Exp)) {
      if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
        if (V->getType()->isCheckedPointerNtArrayType()) {
          unsigned NewBounds = 1;
          if (!EB->Gen[succ].count(V))
            EB->Gen[succ].insert(std::make_pair(V, NewBounds));
          else {
            NewBounds = std::max(NewBounds, EB->Gen[succ][V]);
            EB->Gen[succ][V] = NewBounds;
          }
        }

    // For conditions of the form "if (*(p + i))"
    } else if (const auto *BO = dyn_cast<BinaryOperator>(Exp)) {
      if (BO->getOpcode() != BO_Add)
        return;

      const auto *D =
        dyn_cast<DeclRefExpr>(IgnoreCasts(BO->getLHS()));
      const auto *Lit = dyn_cast<IntegerLiteral>(IgnoreCasts(BO->getRHS()));
      if (!D || !Lit)
        return;

      if (const auto *V = dyn_cast<VarDecl>(D->getDecl())) {
        if (V->getType()->isCheckedPointerNtArrayType()) {
          unsigned NewBounds = 1 + Lit->getValue().getLimitedValue();
          if (!EB->Gen[succ].count(V))
            EB->Gen[succ].insert(std::make_pair(V, NewBounds));
          else {
            NewBounds = std::max(NewBounds, EB->Gen[succ][V]);
            EB->Gen[succ][V] = NewBounds;
          }
        }
      }
    }

  // Handle if conditions of the form "if (*e1 && *e2)".
  } else if (const auto *BO = dyn_cast<const BinaryOperator>(E)) {
      if (ContainsPointerDeref(BO->getLHS()))
        FillGenSet(BO->getLHS(), EB, succ);
      if (ContainsPointerDeref(BO->getRHS()))
        FillGenSet(BO->getRHS(), EB, succ);
  }
}

void BoundsAnalysis::ComputeKillSets(BlockMapTy BlockMap) {
  // For a block B, a variable v is added to Kill[B] if v is assigned to in B.

  for (const auto B : BlockMap) {
    auto EB = B.second;
    DeclSetTy DefinedVars;

    for (auto Elem : *(EB->Block))
      if (Elem.getKind() == CFGElement::Statement)
        CollectDefinedVars(Elem.castAs<CFGStmt>().getStmt(), DefinedVars);

    for (const auto V : DefinedVars)
      EB->Kill.insert(V);
  }
}

void BoundsAnalysis::CollectDefinedVars(const Stmt *S, DeclSetTy &DefinedVars) {
  if (!S)
    return;

  Expr *E = nullptr;
  if (const auto *UO = dyn_cast<const UnaryOperator>(S)) {
    if (UO->isIncrementDecrementOp())
      E = IgnoreCasts(UO->getSubExpr());
  } else if (const auto *BO = dyn_cast<const BinaryOperator>(S)) {
    if (BO->isAssignmentOp())
      E = IgnoreCasts(BO->getLHS());
  }

  if (E) {
    if (const auto *D = dyn_cast<DeclRefExpr>(E))
      if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
        if (V->getType()->isCheckedPointerNtArrayType())
          DefinedVars.insert(V);
  }

  for (const auto I : S->children())
    CollectDefinedVars(I, DefinedVars);
}

void BoundsAnalysis::ComputeInSets(ElevatedCFGBlock *EB, BlockMapTy BlockMap) {
  // In[B1] = n Out[B*->B1], where B* are all preds of B1.

  BoundsMapTy Intersections;
  bool ItersectionEmpty = true;

  for (const CFGBlock *pred : EB->Block->preds()) {
    if (SkipBlock(pred))
      continue;

    auto PredEB = BlockMap[pred];

    if (ItersectionEmpty) {
      Intersections = PredEB->Out[EB->Block];
      ItersectionEmpty = false;
    } else
      Intersections = Intersect(Intersections, PredEB->Out[EB->Block]);
  }
  EB->In = Intersections;
}

void BoundsAnalysis::ComputeOutSets(ElevatedCFGBlock *EB,
                                    BlockMapTy BlockMap,
                                    WorkListTy &WorkList) {
  // Out[B1->B2] = (In[B1] - Kill[B1]) u Gen[B1->B2].

  auto Diff = Difference(EB->In, EB->Kill);

  for (const CFGBlock *succ : EB->Block->succs()) {
    if (SkipBlock(succ))
      continue;

    auto OldOut = EB->Out[succ];
    EB->Out[succ] = Union(Diff, EB->Gen[succ]);

    if (Differ(OldOut, EB->Out[succ]))
      WorkList.push(BlockMap[succ]);
  }
}

void BoundsAnalysis::CollectWidenedBounds(BlockMapTy BlockMap) {
  for (auto item : BlockMap) {
    const auto *B = item.first;
    auto *EB = item.second;
    WidenedBounds[B] = EB->In;
    delete EB;
  }
}

BoundsMapTy BoundsAnalysis::GetWidenedBounds(const CFGBlock *B) {
  return WidenedBounds[B];
}

Expr *BoundsAnalysis::GetTerminatorCondition(const CFGBlock *B) const {
  if (const Stmt *S = B->getTerminator())
    if (const auto *IfS = dyn_cast<IfStmt>(S))
      return const_cast<Expr *>(IfS->getCond());
  return nullptr;
}

Expr *BoundsAnalysis::IgnoreCasts(Expr *E) {
  while (E) {
    E = E->IgnoreParens();

    if (isa<ParenExpr>(E)) {
      E = E->IgnoreParenCasts();
      continue;
    }

    if (isa<ImplicitCastExpr>(E)) {
      E = E->IgnoreImplicit();
      continue;
    }

    if (auto *CE = dyn_cast<CastExpr>(E)) {
      E = CE->getSubExpr();
      continue;
    }

    return E;
  }
  return E;
}


bool BoundsAnalysis::IsPointerDerefLValue(Expr *E) const {
  if (const auto *UO = dyn_cast<UnaryOperator>(E))
    return UO->getOpcode() == UO_Deref;
  return false;
}

bool BoundsAnalysis::ContainsPointerDeref(Expr *E) const {
  if (auto *CE = dyn_cast<CastExpr>(E)) {
    if (CE->getCastKind() == CastKind::CK_LValueToRValue)
      return IsPointerDerefLValue(CE->getSubExpr());
    return ContainsPointerDeref(CE->getSubExpr());
  }

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    return ContainsPointerDeref(BO->getLHS()) ||
           ContainsPointerDeref(BO->getRHS());
  }

  return false;
}

template<class T>
T BoundsAnalysis::Intersect(T &A, T &B) const {
  if (!A.size())
    return A;

  auto Ret = A;
  if (!B.size()) {
    Ret.clear();
    return Ret;
  }

  for (auto I = Ret.begin(), E = Ret.end(); I != E;) {
    if (!B.count(I->first)) {
      auto Next = std::next(I);
      Ret.erase(I);
      I = Next;
    } else {
      Ret[I->first] = std::min(Ret[I->first], B[I->first]);
      ++I;
    }
  }
  return Ret;
}

template<class T>
T BoundsAnalysis::Union(T &A, T &B) const {
  if (!A.size())
    return B;

  auto Ret = A;
  for (const auto item : B) {
    if (!Ret.count(item.first))
      Ret[item.first] = item.second;
    else
      Ret[item.first] = std::max(Ret[item.first], item.second);
  }
  return Ret;
}

template<class T, class U>
T BoundsAnalysis::Difference(T &A, U &B) const {
  if (!A.size() || !B.size())
    return A;

  auto Ret = A;
  for (auto I = Ret.begin(), E = Ret.end(); I != E;) {
    if (B.count(I->first)) {
      auto Next = std::next(I);
      Ret.erase(I);
      I = Next;
    } else ++I;
  }
  return Ret;
}

template<class T>
bool BoundsAnalysis::Differ(T &A, T &B) const {
  if (A.size() != B.size())
    return true;
  auto Ret = Intersect(A, B);
  return Ret.size() != A.size();
}

OrderedBlocksTy BoundsAnalysis::GetOrderedBlocks() {
  // WidenedBounds is a DenseMap and hence is not suitable for iteration as its
  // iteration order is non-deterministic. So we first need to order the
  // blocks. The block IDs decrease from entry to exit. So we sort in the
  // reverse order.
  OrderedBlocksTy OrderedBlocks;
  for (auto item : WidenedBounds)
    OrderedBlocks.push_back(item.first);

  llvm::sort(OrderedBlocks.begin(), OrderedBlocks.end(),
             [] (const CFGBlock *A, const CFGBlock *B) {
               return A->getBlockID() > B->getBlockID();
             });
  return OrderedBlocks;
}

bool BoundsAnalysis::SkipBlock(const CFGBlock *B) const {
  return B == &Cfg->getEntry() || B == &Cfg->getExit();
}

void BoundsAnalysis::DumpWidenedBounds(FunctionDecl *FD) {
  llvm::outs() << "--------------------------------------\n";
  llvm::outs() << "In function: " << FD->getName() << "\n";

  for (const auto *B : GetOrderedBlocks()) {
    llvm::outs() << "--------------------------------------";
    B->print(llvm::outs(), Cfg, S.getLangOpts(), /* ShowColors */ true);

    // WidenedBounds[B] is a MapVector whose iteration order is the same as the
    // insertion order. So we can deterministically iterate the VarDecls.
    for (auto item : WidenedBounds[B])
      llvm::outs() << "upper_bound("
                   << item.first->getNameAsString() << ") = "
                   << item.second << "\n";
  }
}

} // end namespace clang
