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

using namespace llvm;

namespace clang {
// Legend:
// n ==> intersection.
// E ==> belongs.
// u ==> union.

void BoundsAnalysis::WidenBounds() {
  assert(Cfg && "expected CFG to exist");

  WorkListTy Worklist;
  BlockMapTy BlockMap;

  // Add each block to Worklist and create a mapping from Block to
  // ElevatedCFGBlock.
  auto POView = PostOrderCFGView(Cfg);
  for (const auto *B : POView) {
    auto EB = new ElevatedCFGBlock(B);
    Worklist.insert(EB);
    BlockMap[B] = EB;
  }

  // Compute Gen map for each ElevatedCFGBlock.
  for (const auto *B : POView)
    UpdateGenMap(BlockMap[B], BlockMap);

  // Dataflow analysis for bounds widening.
  while (!Worklist.empty()) {
    auto *EB = Worklist.back();
    Worklist.pop_back();

    UpdateInMap(EB, BlockMap);
    auto OldOut = UpdateOutMap(EB);

    // Add the successors of the changed blocks to the worklist, if they do not
    // already exist in the worklist.
    if (Differ(OldOut, EB->Out)) {
      for (const CFGBlock *succ : EB->Block->succs())
        // Worklist is a SetVector. So it will only insert a key if it does not
        // already exist.
        Worklist.insert(BlockMap[succ]);
    }
  }

  CollectWidenedBounds(BlockMap);
}

void BoundsAnalysis::UpdateGenMap(ElevatedCFGBlock *EB, BlockMapTy BlockMap) {
  // Gen(B) = n Gen(B'), where B' E preds(B)
  //          + 1, if all B' branch to B on a condition of the form "if *p".
  //            0, otherwise.
  BoundsMap Intersections;
  bool ItersectionEmpty = true;

  // DeclMap stores the number of preds of EB which branch to EB on a condition
  // like "if *p". If all the preds of EB do this then we can increment
  // EB->Gen[D] by 1.
  using DeclMapTy = llvm::DenseMap<const VarDecl *, unsigned>;
  DeclMapTy DeclMap;

  for (const CFGBlock *pred : EB->Block->preds()) {
    auto PredEB = BlockMap[pred];
    if (ItersectionEmpty) {
      Intersections = PredEB->Gen;
      ItersectionEmpty = false;
    } else
      Intersections = Intersect(Intersections, PredEB->Gen);

    // Fill DeclMap.
    if (const Expr *E = GetTerminatorCondition(pred)) {
      if (!ContainsPointerDeref(E))
        continue;

      // We can update EB->Gen only if EB is the then block of the condition
      // "if *p". For example:

      // B1: if (*p)
      // B2:   foo();
      // B3: else bar();

      //      B1
      //     /  \
      //    B2   B3

      // B1: preds[], succs[B2, B3]
      // B2: preds[B1], succs[]
      // B3: preds[B1], succs[]

      // This means for an if condition, the then block is always the first
      // block in the list of succs. We use this fact to check if EB is the
      // then block of the if condition.
      if (const auto *I = pred->succs().begin())
        if (*I != EB->Block)
          continue;

      const VarDecl *D = GetVarDecl(E);
      if (D && D->getType()->isCheckedPointerNtArrayType()) {
        if (!DeclMap.count(D))
          DeclMap[D] = 1;
        else
          DeclMap[D]++;
      }
    }
  }
  EB->Gen = Intersections;

  // Now update EB->Gen.
  for (const auto item : DeclMap) {
    unsigned count = item.second;
    if (count == EB->Block->pred_size()) {
      const auto *D = item.first;
      if (!EB->Gen.count(D))
        EB->Gen[D] = 1;
      else
        EB->Gen[D]++;
    }
  }
}

void BoundsAnalysis::UpdateInMap(ElevatedCFGBlock *EB, BlockMapTy BlockMap) {
  // In(B) = min(n Out(B')), where B E preds(B).

  // In(B) is the intersection of the Out's of all preds of B. For the
  // intersection, we pick the Decl with the smaller upper bound. For example,
  // if preds(B) = {X, Y} and Out(X)= {p:1, q:2, r:0} and Out(Y) = {p:0, q:3,
  // s:2} then In(B) = {p:0, q:2}.
  BoundsMap Intersections;
  bool ItersectionEmpty = true;

  for (const CFGBlock *pred : EB->Block->preds()) {
    auto PredEB = BlockMap[pred];
    if (ItersectionEmpty) {
      Intersections = PredEB->Out;
      ItersectionEmpty = false;
    } else
      Intersections = Intersect(Intersections, PredEB->Out);
  }
  EB->In = Intersections;
}

BoundsMap BoundsAnalysis::UpdateOutMap(ElevatedCFGBlock *EB) {
  // Out(B) = min(Gen(B) u In(B)).

  // Out(B) is the union of In(B) and Gen(B). If both In and Gen have the same
  // Decl then for the union we pick the one with the larger upper bound. For
  // example, if In(B) = {p:1, q:2} and Gen(B) = {p:2}, then Out(B) = {p:2,
  // q:2}.
  auto OldOut = EB->Out;
  EB->Out = Union(EB->In, EB->Gen);
  return OldOut;
}

void BoundsAnalysis::CollectWidenedBounds(BlockMapTy BlockMap) {
  for (auto item : BlockMap) {
    const auto *B = item.first;
    auto *EB = item.second;
    WidenedBounds[B] = EB->Out;
  }
}

BoundsMap BoundsAnalysis::GetWidenedBounds(const CFGBlock *B) {
  return WidenedBounds[B];
}

const Expr *BoundsAnalysis::GetTerminatorCondition(const CFGBlock *B) const {
  // TODO: Handle other Stmt types, like while loops, etc.
  if (const Stmt *S = B->getTerminator())
    if (const auto *IfS = dyn_cast<IfStmt>(S))
      return IfS->getCond();
  return nullptr;
}

const VarDecl *BoundsAnalysis::GetVarDecl(const Expr *E) const {
  if (const auto *CE = dyn_cast<CastExpr>(E))
    if (const auto *UO = dyn_cast<UnaryOperator>(CE->getSubExpr()))
      if (auto *D = dyn_cast<DeclRefExpr>(UO->getSubExpr()->IgnoreImplicit()))
        return dyn_cast<VarDecl>(D->getDecl());
  return nullptr;
}

bool BoundsAnalysis::IsPointerDerefLValue(const Expr *E) const {
  if (const auto *UO = dyn_cast<UnaryOperator>(E))
    return UO->getOpcode() == UO_Deref;
  return false;
}

bool BoundsAnalysis::ContainsPointerDeref(const Expr *E) const {
  if (const auto *CE = dyn_cast<CastExpr>(E)) {
    if (CE->getCastKind() != CastKind::CK_LValueToRValue)
      return false;
    return IsPointerDerefLValue(CE->getSubExpr());
  }
  return false;
}

// Note: Intersect, Union and Differ mutate theiirr first argument.
BoundsMap BoundsAnalysis::Intersect(BoundsMap &A, BoundsMap &B) {
  if (!B.size()) {
    A.clear();
    return A;
  }

  for (auto I = A.begin(), E = A.end(); I != E; ++I) {
    if (!B.count(I->first)) {
      auto Next = std::next(I);
      A.erase(I);
      I = Next;
    } else
      A[I->first] = std::min(A[I->first], B[I->first]);
  }
  return A;
}

BoundsMap BoundsAnalysis::Union(BoundsMap &A, BoundsMap &B) {
  for (const auto item : B) {
    if (!A.count(item.first))
      A[item.first] = item.second;
    else
      A[item.first] = std::max(A[item.first], item.second);
  }
  return A;
}

bool BoundsAnalysis::Differ(BoundsMap &A, BoundsMap &B) {
  if (A.size() != B.size())
    return true;
  auto OldA = A;
  A = Intersect(A, B);
  return A.size() != OldA.size();
}

} // end namespace clang
