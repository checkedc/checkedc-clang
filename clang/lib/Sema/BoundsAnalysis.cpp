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

void BoundsAnalysis::WidenBounds(FunctionDecl *FD) {
  assert(Cfg && "expected CFG to exist");

  WorkListTy WorkList;

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

  // Collect all ntptrs in scope.
  CollectNtPtrsInScope(FD);

  // Compute Gen and Kill sets.
  ComputeGenSets();
  ComputeKillSets();

  // Compute In and Out sets.
  while (!WorkList.empty()) {
    ElevatedCFGBlock *EB = WorkList.next();
    WorkList.remove(EB);

    ComputeInSets(EB);
    ComputeOutSets(EB, WorkList);
  }

  CollectWidenedBounds();
}

void BoundsAnalysis::ComputeGenSets() {
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

  if (IsDeclOperand(E)) {
    const DeclRefExpr *D = GetDeclOperand(E);
    if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
      BoundsVars.insert(V);
  }
}

DeclRefExpr *BoundsAnalysis::GetDeclOperand(const Expr *E) {
  if (!E || !isa<CastExpr>(E))
    return nullptr;
  auto *CE = dyn_cast<CastExpr>(E);
  assert(CE->getSubExpr() && "invalid CastExpr expression");

  return dyn_cast<DeclRefExpr>(IgnoreCasts(CE->getSubExpr()));
}

bool BoundsAnalysis::IsDeclOperand(const Expr *E) {
  if (auto *CE = dyn_cast<CastExpr>(E)) {
    assert(CE->getSubExpr() && "invalid CastExpr expression");

    if (CE->getCastKind() == CastKind::CK_LValueToRValue ||
        CE->getCastKind() == CastKind::CK_ArrayToPointerDecay)
      return isa<DeclRefExpr>(IgnoreCasts(CE->getSubExpr()));
  }
  return false;
}

void BoundsAnalysis::CollectNtPtrsInScope(FunctionDecl *FD) {
  // TODO: Currently, we simply collect all ntptrs defined in the current
  // function. Ultimately, we need to do a liveness analysis of what ntptrs are
  // in scope for a block.

  assert(FD && "invalid function");

  // Collect ntptrs passed as parameters to the current function.
  for (const ParmVarDecl *PD : FD->parameters())
    if (IsNtArrayType(PD))
      NtPtrsInScope.insert(PD);

  // Collect all ntptrs defined in the current function. BlockMap contains all
  // blocks of the current function. We iterate through all blocks in BlockMap
  // and try to gather ntptrs.
  for (const auto item : BlockMap) {
    const CFGBlock *B = item.first;

    for (CFGElement Elem : *B) {
      if (Elem.getKind() != CFGElement::Statement)
        continue;
      const Stmt *S = Elem.castAs<CFGStmt>().getStmt();
      if (!S)
        continue;

      if (const auto *DS = dyn_cast<DeclStmt>(S)) {
        for (const Decl *D : DS->decls())
          if (const auto *V = dyn_cast<VarDecl>(D))
            if (IsNtArrayType(V))
              NtPtrsInScope.insert(V);
      }
    }
  }
}

void BoundsAnalysis::FillGenSetAndGetBoundsVars(const Expr *E,
                                                ElevatedCFGBlock *EB,
                                                ElevatedCFGBlock *SuccEB) {

  // TODO: Handle accesses of the form:
  // "if (*(p + i) && *(p + j) && *(p + k))"

  // The deref expr can be of 2 forms:
  // 1. Ptr deref or array subscript: if (*p) or p[i]
  // 2. BinaryOperator: if (*(p + e))

  // SplitIntoBaseOffset tries to uniformize the expr. It returns a "Base" and
  // an "Offset". Base is an expr containing only DeclRefExprs and Offset is an
  // expr containing only IntegerLiterals. Here are some examples: Note: p and
  // q are DelRefExprs, i and j are IntegerLiterals.

  // Expr ==> Return Value
  // p ==> (p, nullptr)
  // p + i ==> (p, i)
  // i + p ==> (p, i)
  // p + i + j ==> (p, i + j)
  // i + p + j + q ==> (p + q, i + j)

  ExprIntPairTy DerefExprIntPair = SplitIntoBaseOffset(E);
  const Expr *DerefBase = DerefExprIntPair.first;
  if (!DerefBase)
    return;
  llvm::APSInt DerefOffset = DerefExprIntPair.second;

  // For bounds widening, the base of the deref expr and the declared upper
  // bounds expr for all ntptrs in scope should be the same.

  // For example:
  // _Nt_array_ptr<T> p : bounds(p, p + i);
  // _Nt_array_ptr<T> q : bounds(p, p + i);
  // _Nt_array_ptr<T> r : bounds(r, r + i);

  // if (*(p + i)) // widen p and q by 1
  //   if (*(p + i + 1)) // widen p and q by 2
  //     if (*(p + i + 2)) // widen p and q by 3

  // if (*(p + i)) // widen p and q by 1
  //   if (*(p + i + 2)) // no widening

  // if (*p) // no widening
  //   if (*(p + 1)) // no widening
  //     if (*(p + i)) // widen p and q by 1

  // TODO: Currently, Lexicographic::CompareExpr does not understand
  // commutativity of operations. Exprs like "p + e" and "e + p" are considered
  // unequal.

  // TODO: Currently, we iterate and re-compute info for all ntptrs in scope
  // for each ntptr dereference. We can optimize this at the cost of space by
  // storing the VarDecls, variables used in bounds exprs and base/offset for
  // the declared upper bounds expr for the VarDecl. Then we simply have to
  // look this up instead of re-computing.

  for (const VarDecl *V : NtPtrsInScope) {
    // In case the bounds expr for V is not a RangeBoundsExpr, invoke
    // ExpandBoundsToRange to expand it to RangeBoundsExpr.
    const BoundsExpr *BE = S.ExpandBoundsToRange(V, V->getBoundsExpr());
    const auto *RBE = dyn_cast<RangeBoundsExpr>(BE);

    if (!RBE)
      continue;

    // Collect all variables involved in the upper and lower bounds exprs for
    // the ntptr. An assignment to any such variable would kill the widenend
    // bounds for the ntptr.
    if (!SuccEB->BoundsVars.count(V)) {
      DeclSetTy BoundsVars;
      CollectBoundsVars(RBE, BoundsVars);
      SuccEB->BoundsVars[V] = BoundsVars;
    }

    // Update the bounds of p on the edge EB->SuccEB only if we haven't already
    // updated them.
    if (EB->Gen[SuccEB->Block].count(V))
      continue;

    const Expr *UE = RBE->getUpperExpr();
    assert(UE && "invalid upper bounds expr");

    // Split the upper bounds expr into base and offset for matching with the
    // DerefBase.
    ExprIntPairTy UpperExprIntPair = SplitIntoBaseOffset(UE);
    const Expr *UpperBase = UpperExprIntPair.first;
    if (!UpperBase)
      continue;
    llvm::APSInt UpperOffset = UpperExprIntPair.second;

    if (Lex.CompareExpr(DerefBase, UpperBase) !=
        Lexicographic::Result::Equal)
      continue;

    // We cannot widen the bounds if the offset in the deref expr is less than
    // the offset in the declared upper bounds expr. For example:
    // _Nt_array_ptr<T> p : bounds(p, p + i + 2); // Upper Bounds Offset (UBO) = 2.

    // if (*(p + i)) // Offset = 0 < UBO ==> no widening
    //   if (*(p + i + 1)) // Offset = 1 < UBO ==> no widening
    //     if (*(p + i + 2)) // Offset = 2 == UBO ==> widen by 1
    //       if (*(p + i + 3)) // Offset = 3 > UBO ==> widen by 2

    // We also should not widen the bounds if the pointer dereferences are not
    // sequential. For example:
    // _Nt_array_ptr<T> p : bounds(p, p);

    // if (*p) // Widen by 1
    //   if (*(p + 1)) // Widen by 2
    //     if (*(p + 3)) // No widening because we have not tested *(p + 2).
    // We handle this while computing the union of the In and Gen sets.
    // We widen p by 1 only if the bounds of p in (In - Kill) == Gen[p].
    // See the comments in ComputeOutSets for more details.

    if (llvm::APSInt::compareValues(DerefOffset, UpperOffset) < 0)
      continue;

    // (DerefOffset - UpperOffset) gives the offset of the memory dereference
    // relative to the declared upper bound expression. This offset is used in
    // the widening computation in ComputeOutSets.

    // TODO: Check to see if that difference overflows/underflows.
    EB->Gen[SuccEB->Block][V] = (DerefOffset - UpperOffset).getLimitedValue();
  }
}

ExprIntPairTy BoundsAnalysis::SplitIntoBaseOffset(const Expr *E) {
  // In order to make an expression uniform, we want to keep all DeclRefExprs
  // on the LHS and all IntegerLiterals on the RHS.

  llvm::APSInt Zero (Ctx.getTypeSize(Ctx.IntTy), 0);

  if (!E)
    return std::make_pair(nullptr, Zero);;

  if (IsDeclOperand(E))
    return std::make_pair(GetDeclOperand(E), Zero);

  if (!isa<BinaryOperator>(E))
    return std::make_pair(nullptr, Zero);;

  const BinaryOperator *BO = dyn_cast<BinaryOperator>(E);
  // TODO: Currently we only handle exprs containing additive operations.
  if (BO->getOpcode() != BO_Add)
    return std::make_pair(nullptr, Zero);;

  Expr *LHS = BO->getLHS()->IgnoreParens();
  Expr *RHS = BO->getRHS()->IgnoreParens();

  assert((LHS && RHS) && "invalid BinaryOperator expression");

  // Note: Assume p, q, r are DeclRefExprs and i, j are IntegerLiterals.

  // Case 1: LHS is DeclRefExpr and RHS is IntegerLiteral. This expr is already
  // uniform.
  // p + i ==> return (p, i)
  llvm::APSInt IntVal;

  if (IsDeclOperand(LHS) && RHS->isIntegerConstantExpr(IntVal, Ctx))
    return std::make_pair(GetDeclOperand(LHS), IntVal);

  // Case 2: LHS is IntegerLiteral and RHS is DeclRefExpr. We simply need to
  // swap LHS and RHS to make expr uniform.
  // i + p ==> return (p, i)
  if (LHS->isIntegerConstantExpr(IntVal, Ctx) && IsDeclOperand(RHS))
    return std::make_pair(GetDeclOperand(RHS), IntVal);

  // Case 3: LHS and RHS are both DeclRefExprs. This means there is no
  // IntegerLiteral in the expr. In this case, we return the incoming
  // BinaryOperator expr with a nullptr for the RHS.
  // p + q ==> return (p + q, nullptr)
  if (IsDeclOperand(LHS) && IsDeclOperand(RHS))
    return std::make_pair(BO, Zero);

  // To make parsing simpler, we always try to keep BinaryOperator on the LHS.
  if (!isa<BinaryOperator>(LHS) && isa<BinaryOperator>(RHS))
    std::swap(LHS, RHS);

  // By this time the LHS should be a BinaryOperator; either because it already
  // was a BinaryOperator, or because the RHS was a BinaryOperator and was
  // swapped with the LHS.
  if (!isa<BinaryOperator>(LHS))
    return std::make_pair(nullptr, Zero);;

  // If we reach here, the expr is one of these:
  // Case 4: (p + q) + i
  // Case 5: (p + j) + i
  // Case 6: (p + q) + r
  // Case 7: (p + j) + r

  // TODO: Currently we assume that the RHS cannot be a BinaryOperator. So we
  // do not handle cases like: (p + q) + (i + j).

  auto *BE = dyn_cast<BinaryOperator>(LHS);

  // Recursively, make the LHS uniform.
  ExprIntPairTy ExprIntPair = SplitIntoBaseOffset(BE);
  const Expr *BinOpLHS = ExprIntPair.first;
  llvm::APSInt BinOpRHS = ExprIntPair.second;

  // Expr is either Case 4 or Case 5 from above. ie: LHS is BinaryOperator
  // and RHS is IntegerLiteral.
  // (p + q) + i OR (p + j) + i
  if (RHS->isIntegerConstantExpr(IntVal, Ctx)) {

    // Expr is Case 4. ie: The BinaryOperator expr does not have an
    // IntegerLiteral on the RHS.
    // (p + q) + i ==> return (p + q, i)
    if (llvm::APSInt::compareValues(BinOpRHS, Zero) == 0)
      return std::make_pair(BE, IntVal);

    // Expr is Case 5. ie: The BinaryOperator expr has an IntegerLiteral on
    // the RHS.
    // (p + j) + i ==> return (p, j + i)

    // TODO: Since we are reassociating integers here, check if the value
    // overflows/underflows.
    return std::make_pair(BinOpLHS, IntVal + BinOpRHS);
  }

  // If we are here it means expr is either Case 6 or Case 7 from above. ie:
  // LHS is BinaryOperator and RHS is a DeclRefExpr.
  // (p + q) + r OR (p + j) + r

  // Expr is Case 6. ie: The BinaryOperator expr does not have an
  // IntegerLiteral on the RHS.
  // (p + q) + r ==> return (p + q + r, nullptr)
  if (llvm::APSInt::compareValues(BinOpRHS, Zero) == 0)
    return std::make_pair(BO, Zero);

  // Expr is Case 7. ie: The BinaryOperator expr has an IntegerLiteral on
  // the RHS.
  // (p + j) + r ==> return (p + r, j) OR
  // (j + p) + r ==> return (r + p, j)

  // TmpBE is either (p + j) or (j + p) and RHS is r.
  auto *TmpBE =
    new (Ctx) BinaryOperator(BE->getLHS(), BE->getRHS(), BE->getOpcode(),
                             BE->getType(), BE->getValueKind(),
                             BE->getObjectKind(), BE->getExprLoc(),
                             BE->getFPFeatures());

  // TmpBE is (j + p) and RHS is r. So make the TmpBE as (r + p).
  assert(TmpBE->getLHS() && "invalid BinaryOperator expression");

  if (TmpBE->getLHS()->isIntegerConstantExpr(IntVal, Ctx))
    TmpBE->setLHS(RHS);
  // TmpBE is (p + j) and RHS is r. So make the TmpBE as (p + r).
  else
    TmpBE->setRHS(RHS);
  return std::make_pair(TmpBE, BinOpRHS);
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
  if (auto *AE = dyn_cast<ArraySubscriptExpr>(E)) {
    // An array access can be written A[4] or 4[A] (both are equivalent).
    // getBase() and getIdx() always present the normalized view: A[4].
    // In this case getBase() returns "A" and getIdx() returns "4".
    const auto *BO =
      new (Ctx) BinaryOperator(AE->getBase(), AE->getIdx(),
                               BinaryOperatorKind::BO_Add, AE->getType(),
                               AE->getValueKind(), AE->getObjectKind(),
                               AE->getExprLoc(), FPOptions());

    FillGenSetAndGetBoundsVars(BO, EB, SuccEB);

  } else if (auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() == UO_Deref) {
      assert(UO->getSubExpr() && "invalid UnaryOperator expression");

      const Expr *UE = IgnoreCasts(UO->getSubExpr());
      FillGenSetAndGetBoundsVars(UE, EB, SuccEB);
    }
  }
}

void BoundsAnalysis::ComputeKillSets() {
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
    if (UO->isIncrementDecrementOp()) {
      assert(UO->getSubExpr() && "invalid UnaryOperator expression");
      E = IgnoreCasts(UO->getSubExpr());
    }
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

void BoundsAnalysis::ComputeInSets(ElevatedCFGBlock *EB) {
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

void BoundsAnalysis::CollectWidenedBounds() {
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

bool BoundsAnalysis::SkipBlock(const CFGBlock *B) const {
  return !B || B == &Cfg->getEntry() || B == &Cfg->getExit();
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
  for (auto item : WidenedBounds) {
    // item.first is the CFGBlock.
    OrderedBlocks.push_back(item.first);
  }

  llvm::sort(OrderedBlocks.begin(), OrderedBlocks.end(),
             [] (const CFGBlock *A, const CFGBlock *B) {
               return A->getBlockID() > B->getBlockID();
             });
  return OrderedBlocks;
}

void BoundsAnalysis::DumpWidenedBounds(FunctionDecl *FD) {
  llvm::outs() << "--------------------------------------\n";
  llvm::outs() << "In function: " << FD->getName() << "\n";

  for (const CFGBlock *B : GetOrderedBlocks()) {
    llvm::outs() << "--------------------------------------";
    B->print(llvm::outs(), Cfg, S.getLangOpts(), /* ShowColors */ true);

    BoundsMapTy Vars = WidenedBounds[B];
    using VarPairTy = std::pair<const VarDecl *, unsigned>;

    std::sort(Vars.begin(), Vars.end(), [](VarPairTy A, VarPairTy B) {
                return A.first->getQualifiedNameAsString().compare(
                       B.first->getQualifiedNameAsString()) < 0;
              });

    for (auto item : Vars) {
      llvm::outs() << "upper_bound("
                   << item.first->getQualifiedNameAsString() << ") = "
                   << item.second << "\n";
    }
  }
}

} // end namespace clang
