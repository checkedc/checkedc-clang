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

void BoundsAnalysis::WidenBounds() {
  assert(Cfg && "expected CFG to exist");

  WorkListTy WorkList;
  BlockMapTy BlockMap;

  // Add each block to WorkList and create a mapping from Block to
  // ElevatedCFGBlock.
  // Note: By default, PostOrderCFGView iterates in reverse order. So we always
  // get a reverse post order when we iterate PostOrderCFGView.
  for (const CFGBlock *B : PostOrderCFGView(Cfg)) {
    // SkipBlock will skip all null, entry and exit blocks. PostOrderCFGView
    // does not contain any unreachable blocks. So at the end of this loop
    // BlockMap only contains reachable blocks.
    if (SkipBlock(B))
      continue;

    auto EB = new ElevatedCFGBlock(B);
    // Note: WorkList is a queue. So we maintain the reverse post order when we
    // iterate WorkList.
    WorkList.append(EB);
    BlockMap[B] = EB;
  }

  // At this time, BlockMap only contains reachable blocks. We iterate through
  // all blocks in the CFG and append all unreachable blocks to the WorkList.
  for (auto I = Cfg->begin(), E = Cfg->end(); I != E; ++I) {
    const CFGBlock *B = *I;
    if (!SkipBlock(B) && !BlockMap.count(B)) {
      auto EB = new ElevatedCFGBlock(B);
      WorkList.append(EB);
      BlockMap[B] = EB;
    }
  }

  // Compute Gen and Kill sets.
  ComputeGenSets(BlockMap);
  ComputeKillSets(BlockMap);

  // Compute In and Out sets.
  while (!WorkList.empty()) {
    ElevatedCFGBlock *EB = WorkList.next();
    WorkList.remove(EB);

    ComputeInSets(EB, BlockMap);
    ComputeOutSets(EB, BlockMap, WorkList);
  }

  CollectWidenedBounds(BlockMap);
}

void BoundsAnalysis::ComputeGenSets(BlockMapTy BlockMap) {
  // If there is an edge B1->B2 and the edge condition is of the form
  // "if (*(p + i))" then Gen[B1] = {B2, p:i} .

  for (const auto item : BlockMap) {
    ElevatedCFGBlock *EB = item.second;

    for (const CFGBlock *pred : EB->Block->preds()) {
      if (SkipBlock(pred))
        continue;

      // We can add "p:i" only on the true edge.
      // For example,
      // B1: if (*(p + i))
      // B2:   foo();
      // B3: else bar();

      // Here we have the edges (B1->B2) and (B1->B3). We can add "p:i" only
      // on the true edge. Which means we will add the following entry to
      // Gen[B1]: {B2, p:i}
      if (!pred->succ_empty() && *pred->succs().begin() == EB->Block)
        // Get the edge condition and fill the Gen set.
        if (Expr *E = GetTerminatorCondition(pred))
          FillGenSet(E, BlockMap[pred], EB);
    }
  }
}

void BoundsAnalysis::CollectBoundsVars(const Expr *E, DeclSetTy &BoundsVars) {
  if (!E)
    return;

  E = IgnoreCasts(E);

  // Collect bounds vars for the lower and upper bounds exprs.
  // Example:
  // _Nt_array_ptr<char> p : bounds(p + i, p + j);
  // LowerExpr: p + i.
  // UpperExpr: p + j.
  if (const auto *RBE = dyn_cast<RangeBoundsExpr>(E)) {
    CollectBoundsVars(RBE->getLowerExpr(), BoundsVars);
    CollectBoundsVars(RBE->getUpperExpr(), BoundsVars);
  }

  // Collect bounds vars for the LHS and RHS of binary expressions.
  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    CollectBoundsVars(BO->getLHS(), BoundsVars);
    CollectBoundsVars(BO->getRHS(), BoundsVars);
  }

  if (IsPtrDerefOrArraySubscript(E)) {
    const DeclRefExpr *D = GetDeclRefExpr(E);
    if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
      BoundsVars.insert(V);
  }
}

DeclRefExpr *BoundsAnalysis::GetDeclRefExpr(const Expr *E) {
  auto *CE = dyn_cast<CastExpr>(E);
  return dyn_cast<DeclRefExpr>(IgnoreCasts(CE->getSubExpr()));
}

bool BoundsAnalysis::IsPtrDerefOrArraySubscript(const Expr *E) {
  if (auto *CE = dyn_cast<CastExpr>(E))
    if (CE->getCastKind() == CastKind::CK_LValueToRValue ||
        CE->getCastKind() == CastKind::CK_ArrayToPointerDecay)
      return isa<DeclRefExpr>(IgnoreCasts(CE->getSubExpr()));
  return false;
}

void BoundsAnalysis::HandlePointerDeref(UnaryOperator *UO,
                                        ElevatedCFGBlock *EB,
                                        ElevatedCFGBlock *SuccEB) {

  const Expr *E = IgnoreCasts(UO->getSubExpr());

  // TODO: Handle accesses of the form:
  // "if (*(p + i) && *(p + j) && *(p + k))"

  // For conditions of the form "if (*p)".
  if (IsPtrDerefOrArraySubscript(E)) {
    const DeclRefExpr *D = GetDeclRefExpr(E);

    if (const auto *V = dyn_cast<VarDecl>(D->getDecl())) {
      if (IsNtArrayType(V)) {
        EB->Gen[SuccEB->Block].insert(std::make_pair(V, 0));
        if (!SuccEB->BoundsVars.count(V)) {
          DeclSetTy BoundsVars;
          CollectBoundsVars(UO->getBoundsExpr(), BoundsVars);
          SuccEB->BoundsVars[V] = BoundsVars;
        }
      }
    }

  // For conditions of the form "if (*(p + i))"
  } else if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    // Currently we only handle additive offsets.
    if (BO->getOpcode() != BO_Add)
      return;

    Expr *LHS = IgnoreCasts(BO->getLHS());
    Expr *RHS = IgnoreCasts(BO->getRHS());
    DeclRefExpr *D = nullptr;
    IntegerLiteral *Lit = nullptr;

    // Handle *(p + i).
    if (IsPtrDerefOrArraySubscript(LHS) && isa<IntegerLiteral>(RHS)) {
      D = GetDeclRefExpr(LHS);
      Lit = dyn_cast<IntegerLiteral>(RHS);

    // Handle *(i + p).
    } else if (IsPtrDerefOrArraySubscript(RHS) && isa<IntegerLiteral>(LHS)) {
      D = GetDeclRefExpr(RHS);
      Lit = dyn_cast<IntegerLiteral>(LHS);
    }

    if (!D || !Lit)
      return;

    if (const auto *V = dyn_cast<VarDecl>(D->getDecl())) {
      if (IsNtArrayType(V)) {
        // We update the bounds of p on the edge EB->SuccEB only if this is
        // the first time we encounter "if (*(p + i)" on that edge.
        if (!EB->Gen[SuccEB->Block].count(V)) {
          EB->Gen[SuccEB->Block][V] = Lit->getValue().getLimitedValue();
          if (!SuccEB->BoundsVars.count(V)) {
            DeclSetTy BoundsVars;
            CollectBoundsVars(UO->getBoundsExpr(), BoundsVars);
            SuccEB->BoundsVars[V] = BoundsVars;
          }
        }
      }
    }
  }
}

void BoundsAnalysis::HandleArraySubscript(ArraySubscriptExpr *AE,
                                          ElevatedCFGBlock *EB,
                                          ElevatedCFGBlock *SuccEB) {

  // An array access can be written A[4] or 4[A] (both are equivalent).
  // getBase() and getIdx() always present the normalized view: A[4].
  // In this case getBase() returns "A" and getIdx() returns "4".
  const Expr *Base = AE->getBase();
  const Expr *Index = AE->getIdx();

  if (IsPtrDerefOrArraySubscript(Base)) {
    const DeclRefExpr *D = GetDeclRefExpr(Base);

    if (const auto *V = dyn_cast<VarDecl>(D->getDecl())) {
      if (IsNtArrayType(V)) {
        Expr::EvalResult Res;
        if (!Index->EvaluateAsInt(Res, S.Context))
          return;

        llvm::APSInt Idx = Res.Val.getInt();
        if (Idx.isNegative())
          return;

        EB->Gen[SuccEB->Block].insert(std::make_pair(V, Idx.getLimitedValue()));

        if (!SuccEB->BoundsVars.count(V)) {
          DeclSetTy BoundsVars;
          CollectBoundsVars(AE->getBoundsExpr(), BoundsVars);
          SuccEB->BoundsVars[V] = BoundsVars;
        }
      }
    }
  }
}

void BoundsAnalysis::FillGenSet(Expr *E,
                                ElevatedCFGBlock *EB,
                                ElevatedCFGBlock *SuccEB) {

  // Handle if conditions of the form "if (*e1 && *e2)".
  if (const auto *BO = dyn_cast<const BinaryOperator>(E)) {
    if (BO->getOpcode() == BO_LAnd) {
      FillGenSet(BO->getLHS(), EB, SuccEB);
      FillGenSet(BO->getRHS(), EB, SuccEB);
    }
  }

  if (auto *CE = dyn_cast<CastExpr>(E))
    if (CE->getCastKind() == CastKind::CK_LValueToRValue)
      E = CE->getSubExpr();

  E = IgnoreCasts(E);

  // Fill the Gen set based on whether the edge condition is an array subscript
  // or a pointer deref.
  if (auto *AS = dyn_cast<ArraySubscriptExpr>(E))
    HandleArraySubscript(AS, EB, SuccEB);
  else if (auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() == UO_Deref)
      HandlePointerDeref(UO, EB, SuccEB);
  }
}

void BoundsAnalysis::ComputeKillSets(BlockMapTy BlockMap) {
  // For a block B, a variable v is added to Kill[B] if v is assigned to in B.

  for (const auto item : BlockMap) {
    ElevatedCFGBlock *EB = item.second;
    DeclSetTy DefinedVars;

    for (CFGElement Elem : *(EB->Block))
      if (Elem.getKind() == CFGElement::Statement)
        CollectDefinedVars(Elem.castAs<CFGStmt>().getStmt(), EB, DefinedVars);

    for (const VarDecl *V : DefinedVars)
      EB->Kill.insert(V);
  }
}

void BoundsAnalysis::CollectDefinedVars(const Stmt *S, ElevatedCFGBlock *EB,
                                        DeclSetTy &DefinedVars) {
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
    if (const auto *D = dyn_cast<DeclRefExpr>(E)) {
      if (const auto *V = dyn_cast<VarDecl>(D->getDecl())) {
        if (IsNtArrayType(V))
          DefinedVars.insert(V);
        else {

          // BoundsVars is a mapping from _Nt_array_ptrs to all the variables
          // used in their bounds exprs. For example:

          // _Nt_array_ptr<char> p : bounds(p + i, i + p + j + 10);
          // _Nt_array_ptr<char> q : bounds(i + q, i + p + q + m);

          // EB->BoundsVars: {p: {p, i, j}, q: {i, q, p, m}}

          for (auto item : EB->BoundsVars) {
            if (item.second.count(V))
              DefinedVars.insert(item.first);
          }
        }
      }
    }
  }

  for (const Stmt *St : S->children())
    CollectDefinedVars(St, EB, DefinedVars);
}

void BoundsAnalysis::ComputeInSets(ElevatedCFGBlock *EB, BlockMapTy BlockMap) {
  // In[B1] = n Out[B*->B1], where B* are all preds of B1.

  BoundsMapTy Intersections;
  bool ItersectionEmpty = true;

  for (const CFGBlock *pred : EB->Block->preds()) {
    if (SkipBlock(pred))
      continue;

    ElevatedCFGBlock *PredEB = BlockMap[pred];

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

  BoundsMapTy Diff = Difference(EB->In, EB->Kill);
  for (const CFGBlock *succ : EB->Block->succs()) {
    if (SkipBlock(succ))
      continue;

    BoundsMapTy OldOut = EB->Out[succ];

    // Here's how we compute (In - Kill) u Gen:

    // 1. If variable p does not exist in (In - Kill), then
    // (Gen[p] == 0) ==> Out[B1->B2] = {p:1}.
    // In other words, if p does not exist in (In - Kill) it means that p is
    // dereferenced for the first time on the incoming edge to this block, like
    // "if (*p)". So we can initialize the bounds of p to 1. But we may also
    // run into cases like "if (*(p + 100))". In this case, we cannot
    // initialize the bounds of p. So additionally we check if Gen[p] == 0.

    // 2. Else if the bounds of p in (In - Kill) == Gen[V] then widen the
    // bounds of p by 1.
    // Consider this example:
    // B1: if (*p) { // In[B1] = {}, Gen[Entry->B1] = {} ==> bounds(p) = 1.
    // B2:   if (*(p + 1)) { // In[B2] = {p:1}, Gen[B1->B2] = {p:1} ==> bounds(p) = 2.
    // B3:     if (*(p + 2)) { // In[B2] = {p:2}, Gen[B1->B2] = {p:2} ==> bounds(p) = 3.

    EB->Out[succ] = Union(Diff, EB->Gen[succ]);

    if (Differ(OldOut, EB->Out[succ]))
      WorkList.append(BlockMap[succ]);
  }
}

void BoundsAnalysis::CollectWidenedBounds(BlockMapTy BlockMap) {
  for (auto item : BlockMap) {
    const CFGBlock *B = item.first;
    ElevatedCFGBlock *EB = item.second;
    WidenedBounds[B] = EB->In;
    delete EB;
  }
}

BoundsMapTy BoundsAnalysis::GetWidenedBounds(const CFGBlock *B) {
  return WidenedBounds[B];
}

Expr *BoundsAnalysis::GetTerminatorCondition(const CFGBlock *B) const {
  if (const Stmt *S = B->getTerminator()) {
    if (const auto *IfS = dyn_cast<IfStmt>(S))
      return const_cast<Expr *>(IfS->getCond());
  }
  return nullptr;
}

Expr *BoundsAnalysis::IgnoreCasts(const Expr *E) {
  return Lex.IgnoreValuePreservingOperations(Ctx, const_cast<Expr *>(E));
}

bool BoundsAnalysis::IsNtArrayType(const VarDecl *V) const {
  return V->getType()->isCheckedPointerNtArrayType() ||
         V->getType()->isNtCheckedArrayType();
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
    const auto *V = I->first;

    if (!B.count(V)) {
      auto Next = std::next(I);
      Ret.erase(I);
      I = Next;
    } else {
      Ret[V] = std::min(Ret[V], B[V]);
      ++I;
    }
  }
  return Ret;
}

template<class T>
T BoundsAnalysis::Union(T &A, T &B) const {
  auto Ret = A;
  for (const auto item : B) {
    const auto *V = item.first;
    auto I = item.second;

    if (!Ret.count(V)) {
      if (I == 0)
        Ret[V] = 1;
    } else if (I == Ret[V])
      Ret[V] = I + 1;
  }
  return Ret;
}

template<class T, class U>
T BoundsAnalysis::Difference(T &A, U &B) const {
  if (!A.size() || !B.size())
    return A;

  auto Ret = A;
  for (auto I = Ret.begin(), E = Ret.end(); I != E; ) {
    const auto *V = I->first;
    if (B.count(V)) {
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
  return !B || B == &Cfg->getEntry() || B == &Cfg->getExit();
}

void BoundsAnalysis::DumpWidenedBounds(FunctionDecl *FD) {
  llvm::outs() << "--------------------------------------\n";
  llvm::outs() << "In function: " << FD->getName() << "\n";

  for (const CFGBlock *B : GetOrderedBlocks()) {
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
