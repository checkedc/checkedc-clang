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

void BoundsAnalysis::Analyze() {
  llvm::dbgs() << "### Debug bounds analysis\n";
  assert(Cfg && "expected CFG to exist");

  SetVector<ElevatedCFGBlock *> Worklist;
  DenseMap<const CFGBlock *, ElevatedCFGBlock *> BlockMap;

  for (const auto *B : PostOrderCFGView(Cfg)) {
    auto EB = new ElevatedCFGBlock(B);
    Worklist.insert(EB);
    BlockMap[B] = EB;    
  }

  // Dataflow analysis for bounds widening.
  while (!Worklist.empty()) {
    auto *CurrentBlock = Worklist.back();
    Worklist.pop_back();

    auto BlockValid = IsBlockValidForAnalysis(CurrentBlock);
    if (!BlockValid.first)
      continue;

    const Expr *E = BlockValid.second;
    auto *D = getVarDecl(E);
    if (!D || !D->getType()->isCheckedPointerNtArrayType())
      continue;

    auto *BE = S.ExpandToRange(D, D->getBoundsExpr());
    if (!isa<RangeBoundsExpr>(BE))
      continue;

    // Compute Gen set.

    // Update In set.
    // In(B) = { intersection of Out(B') } where B' belongs to preds(B).
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
    // Out(B) = { (In(B) - Kill(B)) union Gen(B) }.
    auto OldOut = CurrentBlock->Out;
    CurrentBlock->Out = set_difference(CurrentBlock->In, CurrentBlock->Kill);
    set_union(CurrentBlock->Out, CurrentBlock->Gen);

    // Add the changed blocks to the worklist.
    set_intersect(OldOut, CurrentBlock->Out);
    if (OldOut.size() != CurrentBlock->Out.size())
      for (const auto B : CurrentBlock->Block->succs())
        // Worklist is a SetVector. So it will only insert a key if it does not
        // already exist.
        Worklist.insert(BlockMap[B]);
  }
}

std::pair<bool, const Expr *>
BoundsAnalysis::IsBlockValidForAnalysis(ElevatedCFGBlock *B) const {
  if (const Stmt *S = B->Block->getTerminator())
    if (const auto *IfS = dyn_cast<IfStmt>(S))
      return std::make_pair(true, IfS->getCond());
  return std::make_pair(false, nullptr);
}

VarDecl *BoundsAnalysis::getVarDecl(const Expr *E) const {
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

} // end namespace clang
