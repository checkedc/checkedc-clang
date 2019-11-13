//===-------- BoundsAnalysis.cpp - collect comparison facts -------===//
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
class Sema;

void BoundsAnalysis::Analyze() {
  assert(Cfg && "expected CFG to exist");

  SetVector<ElevatedCFGBlock *> Worklist;
  DenseMap<const CFGBlock *, ElevatedCFGBlock *> BlockMap;

  for (const auto *B : PostOrderCFGView(Cfg)) {
    auto EB = new ElevatedCFGBlock(B);
    Worklist.insert(EB);
    BlockMap[B] = EB;    
  }

  // Compute Gen and Kill sets.
  for (auto B : Worklist) {
    if (const Stmt *Term = B->Block->getTerminator()) {
      if (const auto *IS = dyn_cast<IfStmt>(Term)) {
        auto *E = IS->getCond();
        // If the if condition derefences a pointer.
        if (ContainsPointerDeref(E)) {
          B->Gen.insert(E);

          if (B->In.count(E))
            B->Kill.insert(E);
        }
      }
    }
  }

  // Iterative worklist algorithm.
  while (!Worklist.empty()) {
    auto *CurrentBlock = Worklist.back();
    Worklist.pop_back();

    // Update In set.
    BoundsSet Intersections;
    bool FirstIteration = true;
    for (const auto B : CurrentBlock->Block->preds()) {
      auto EB = BlockMap[B];
      if (FirstIteration) {
        Intersections = EB->Out;
        FirstIteration = false;
      } else
        set_intersect(Intersections, EB->Out);
    }
    CurrentBlock->In = Intersections;

    // Update Out set.
    auto OldOut = CurrentBlock->Out;
    CurrentBlock->Out = set_difference(CurrentBlock->In, CurrentBlock->Kill);
    set_union(CurrentBlock->Out, CurrentBlock->Gen);

    // Add the changed blocks to the worklist.
    set_intersect(OldOut, CurrentBlock->Out);
    if (OldOut.size() != CurrentBlock->Out.size())
      for (const auto B : CurrentBlock->Block->succs())
        Worklist.insert(BlockMap[B]);
  }
}

bool BoundsAnalysis::IsPointerDerefLValue(const Expr *E) {
  if (const auto *UO = dyn_cast<UnaryOperator>(E))
    return UO->getOpcode() == UO_Deref;
  return false;
}

bool BoundsAnalysis::ContainsPointerDeref(const Expr *E) {
  if (const auto *CE = dyn_cast<CastExpr>(E)) {
    if (CE->getCastKind() != CastKind::CK_LValueToRValue)
      return false;
    return IsPointerDerefLValue(CE->getSubExpr());
  }
  return false;
}

} // end namespace clang
