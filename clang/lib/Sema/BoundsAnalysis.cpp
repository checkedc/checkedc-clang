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
    auto EB = new ElevatedCFGBlock(B);
    WorkList.insert(EB);
    BlockMap[B] = EB;
  }

  // Compute Gen and Kill sets.
  ComputeGenSets(BlockMap);
  ComputeKillSets(BlockMap);

  // Compute In and Out sets.
  while (!WorkList.empty()) {
    auto *EB = WorkList.back();
    WorkList.pop_back();

    ComputeInSets(EB, BlockMap);
    ComputeOutSets(EB,BlockMap, WorkList);
  }

  CollectWidenedBounds(BlockMap);
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

void BoundsAnalysis::ComputeKillSets(BlockMapTy BlockMap) {
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

void BoundsAnalysis::ComputeGenSets(BlockMapTy BlockMap) {
  for (const auto B : BlockMap) {
    auto EB = B.second;
    for (const CFGBlock *pred : EB->Block->preds()) {
      if (Expr *E = GetTerminatorCondition(pred)) {
        if (!ContainsPointerDeref(E))
          continue;
  
        if (const auto *I = pred->succs().begin())
          if (*I != EB->Block)
            continue;

        FillGenSet(E, BlockMap[pred], EB->Block);
      }
    }
  }
}

void BoundsAnalysis::ComputeInSets(ElevatedCFGBlock *EB, BlockMapTy BlockMap) {
  BoundsMapTy Intersections;
  bool ItersectionEmpty = true;

  for (const CFGBlock *pred : EB->Block->preds()) {
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
  auto Diff = Difference(EB->In, EB->Kill);

  for (const CFGBlock *succ : EB->Block->succs()) {
    auto OldOut = EB->Out[succ];
    EB->Out[succ] = Union(Diff, EB->Gen[succ]);

    if (Differ(OldOut, EB->Out[succ]))
      WorkList.insert(BlockMap[succ]);
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
  // TODO: Handle other Stmt types, like while loops, etc.
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

void BoundsAnalysis::FillGenSet(Expr *E, ElevatedCFGBlock *EB,
                                const CFGBlock *succ) {
  E = IgnoreCasts(E);

  if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
    const auto *Exp = IgnoreCasts(UO->getSubExpr());
    if (!Exp)
      return;
 
    // if (*p)
    if (const auto *D = dyn_cast<DeclRefExpr>(Exp)) {
      if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
        if (V->getType()->isCheckedPointerNtArrayType())
          EB->Gen[succ].insert(std::make_pair(V, 1));
      return;
    }
 
    // if (*(p + i))
    if (const auto *BO = dyn_cast<BinaryOperator>(Exp)) {
      if (BO->getOpcode() != BO_Add)
        return;
 
      const auto *D =
        dyn_cast<DeclRefExpr>(IgnoreCasts(BO->getLHS()));
      const auto *Lit = dyn_cast<IntegerLiteral>(IgnoreCasts(BO->getRHS()));
      if (!D || !Lit)
        return;
 
      if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
        if (V->getType()->isCheckedPointerNtArrayType())
          EB->Gen[succ].insert(
            std::make_pair(V, 1 + Lit->getValue().getLimitedValue()));
    }
  }
}

bool BoundsAnalysis::IsPointerDerefLValue(Expr *E) const {
  if (const auto *UO = dyn_cast<UnaryOperator>(E))
    return UO->getOpcode() == UO_Deref;
  return false;
}

bool BoundsAnalysis::ContainsPointerDeref(Expr *E) const {
  if (auto *CE = dyn_cast<CastExpr>(E)) {
    if (CE->getCastKind() != CastKind::CK_LValueToRValue)
      return false;
    return IsPointerDerefLValue(CE->getSubExpr());
  }
  return false;
}

// Note: Intersect, Union and Differ mutate their first argument.
template<class T>
T BoundsAnalysis::Intersect(T &A, T &B) {
  if (!A.size())
    return A;

  if (!B.size()) {
    A.clear();
    return A;
  }

  for (auto I = A.begin(), E = A.end(); I != E;) {
    if (!B.count(I->first)) {
      auto Next = std::next(I);
      A.erase(I);
      I = Next;
    } else {
      A[I->first] = std::min(A[I->first], B[I->first]);
      ++I;
    }
  }
  return A;
}

template<class T>
T BoundsAnalysis::Union(T &A, T &B) {
  for (const auto item : B) {
    if (!A.count(item.first))
      A[item.first] = item.second;
    else
      A[item.first] = std::max(A[item.first], item.second);
  }
  return A;
}

template<class T, class U>
T BoundsAnalysis::Difference(T &A, U &B) {
  if (!A.size() || !B.size())
    return A;

  for (auto I = A.begin(), E = A.end(); I != E;) {
    if (B.count(I->first)) {
      auto Next = std::next(I);
      A.erase(I);
      I = Next;
    } else ++I;
  }
  return A;
}

template<class T>
bool BoundsAnalysis::Differ(T &A, T &B) {
  if (A.size() != B.size())
    return true;
  auto OldA = A;
  A = Intersect(A, B);
  return A.size() != OldA.size();
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
