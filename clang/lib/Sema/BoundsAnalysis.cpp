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
class Sema;

void BoundsAnalysis::WidenBounds() {
  llvm::dbgs() << "### Debug bounds analysis\n";
  assert(Cfg && "expected CFG to exist");

  WorkListTy Worklist;
  BlockMapTy BlockMap;

  // Visit each block and initialize Gen maps.
  for (const auto *B : PostOrderCFGView(Cfg)) {
    auto EB = new ElevatedCFGBlock(B);
    Worklist.insert(EB);
    BlockMap[B] = EB;

    UpdateGenMap(EB, /* Init */ true);
  }

  // Dataflow analysis for bounds widening.
  while (!Worklist.empty()) {
    auto *EB = Worklist.back();
    Worklist.pop_back();

    UpdateGenMap(EB);
    UpdateInMap(EB, BlockMap);
    auto OldOut = UpdateOutMap(EB);

    // Add the changed blocks to the worklist.
    if (Differ(OldOut, EB->Out)) {
      for (const CFGBlock *succ : EB->Block->succs())
        // Worklist is a SetVector. So it will only insert a key if it does not
        // already exist.
        Worklist.insert(BlockMap[succ]);
    }
  }

  // Collect the widened bounds.
  for (auto item : BlockMap) {
    const auto *B = item.first;
    auto *EB = item.second;
    WidenedBounds[B] = EB->In;
  }
}

BoundsMap BoundsAnalysis::GetWidenedBounds(const CFGBlock *B) {
  return WidenedBounds[B];
}

void BoundsAnalysis::UpdateGenMap(ElevatedCFGBlock *EB, bool Init) {
  for (const CFGBlock *pred : EB->Block->preds()) {
    const Expr *E = GetTerminatorCondition(pred);
    if (!E)
      continue;

    auto *D = GetVarDecl(E);
    if (!D || !D->getType()->isCheckedPointerNtArrayType())
      continue;

    if (Init)
      EB->Gen[D] = 0;
    else
      EB->Gen[D]++;
  }
}

void BoundsAnalysis::UpdateInMap(ElevatedCFGBlock *EB, BlockMapTy BlockMap) {
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
  auto OldOut = EB->Out;
  EB->Out = Union(EB->In, EB->Gen);
  return OldOut;
}

const Expr * BoundsAnalysis::GetTerminatorCondition(const CFGBlock *B) const {
  if (const Stmt *S = B->getTerminator())
    if (const auto *IfS = dyn_cast<IfStmt>(S))
      return IfS->getCond();
  return nullptr;
}

VarDecl *BoundsAnalysis::GetVarDecl(const Expr *E) const {
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

BoundsMap BoundsAnalysis::Intersect(BoundsMap &A, BoundsMap &B) {
  if (B.size() == 0) {
    A.clear();
    return A;
  }

  for (auto item : A) {
    if (!B.count(item.first))
      A.erase(item.first);
    else
      A[item.first] = std::min(A[item.first], B[item.first]);
  }
  return A;
}

BoundsMap BoundsAnalysis::Union(BoundsMap &A, BoundsMap &B) {
  for (auto item : B)
    A[item.first] = item.second;
  return A;
}

bool BoundsAnalysis::Differ(BoundsMap &A, BoundsMap &B) {
  if (A.size() != B.size())
    return true;
  auto OldA = A;
  A = Intersect(A, B);
  return A.size() == OldA.size();
}

} // end namespace clang
