//===--------- BoundsAnalysis.cpp - Bounds Widening Analysis --------------===//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements a dataflow analysis for bounds widening.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/BoundsAnalysis.h"

namespace clang {

void BoundsAnalysis::WidenBounds(FunctionDecl *FD, StmtSet NestedStmts) {
  assert(Cfg && "expected CFG to exist");

  WorkListTy WorkList;

  // Add each block to WorkList and create a mapping from Block to
  // ElevatedCFGBlock.
  // Note: By default, PostOrderCFGView iterates in reverse order. So we always
  // get a reverse post order when we iterate PostOrderCFGView.
  for (const CFGBlock *B : PostOrderCFGView(Cfg)) {
    // SkipBlock will skip all null and exit blocks. PostOrderCFGView does not
    // contain any unreachable blocks. So at the end of this loop BlockMap only
    // contains reachable blocks.
    if (SkipBlock(B))
      continue;

    auto EB = new ElevatedCFGBlock(B);
    // Note: WorkList is a queue. So we maintain the reverse post order when we
    // iterate WorkList.
    WorkList.append(EB);
    BlockMap[B] = EB;

    // Mark the In set for the Entry block as "empty". The Out set for the
    // Entry block would be marked as "empty" in ComputeOutSets.
    EB->IsInSetEmpty = B == &Cfg->getEntry();
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
  ComputeKillSets(NestedStmts);

  // Initialize the In and Out sets to Top.
  InitInOutSets();

  // Compute In and Out sets.
  while (!WorkList.empty()) {
    ElevatedCFGBlock *EB = WorkList.next();
    WorkList.remove(EB);

    ComputeInSets(EB);
    ComputeOutSets(EB, WorkList);
  }
}

void BoundsAnalysis::InitInOutSets() {
  for (const auto item : BlockMap) {
    ElevatedCFGBlock *EB = item.second;

    if (EB->Block == &Cfg->getEntry())
      continue;

    EB->In = Top;
    for (const CFGBlock *succ : EB->Block->succs())
      EB->Out[succ] = Top;
  }
}

bool BoundsAnalysis::CheckIsSwitchCaseNull(ElevatedCFGBlock *EB) {
  if (const auto *CS = dyn_cast_or_null<CaseStmt>(EB->Block->getLabel())) {

    // We mimic how clang (in SemaStmt.cpp) gets the value of a switch case. It
    // invokes EvaluateKnownConstInt and we do the same here. SemaStmt has
    // already extended/truncated the case value to fit the integer range and
    // EvaluateKnownConstInt gives us that value.
    llvm::APSInt LHSVal = CS->getLHS()->EvaluateKnownConstInt(Ctx);
    llvm::APSInt LHSZero (LHSVal.getBitWidth(), LHSVal.isUnsigned());
    if (llvm::APSInt::compareValues(LHSVal, LHSZero) == 0)
      return true;

    // Check if the case statement is of the form "case LHS ... RHS" (a GNU
    // extension).
    if (CS->caseStmtIsGNURange()) {
      llvm::APSInt RHSVal = CS->getRHS()->EvaluateKnownConstInt(Ctx);
      llvm::APSInt RHSZero (RHSVal.getBitWidth(), RHSVal.isUnsigned());
      if (llvm::APSInt::compareValues(RHSVal, RHSZero) == 0)
        return true;

      // Check if 0 if contained within the range [LHS, RHS].
      return (LHSVal <= LHSZero && RHSZero <= RHSVal) ||
             (LHSVal >= LHSZero && RHSZero >= RHSVal);
    }
    return false;
  }
  return true;
}

void BoundsAnalysis::ComputeGenSets() {
  // If there is an edge B1->B2 and the edge condition is of the form
  // "if (*(p + i))" then Gen[B1] = {B2, p:i} .

  // Here, EB is B2.
  for (const auto item : BlockMap) {
    ElevatedCFGBlock *EB = item.second;

    // Check if this is a switch case and whether the case label is null.
    // In a switch, we can only widen the bounds in the following cases:
    // 1. Inside a case with a non-null case label.
    // 2. Inside the default case, only if there is another case with a null
    // case label.
    bool IsSwitchCaseNull = CheckIsSwitchCaseNull(EB);

    // Iterate through all preds of EB.
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

      // Check if EB is on a true edge of pred. The false edge (including the
      // default case for a switch) is always the last edge in the list of
      // edges. So we check that EB is not on the last edge for pred.

      if (pred->succ_size() == 0)
        continue;

      // Get the edge condition.
      Expr *E = GetTerminatorCondition(pred);
      if (!E)
        continue;

      const CFGBlock *FalseOrDefaultBlock = *(pred->succs().end() - 1);
      if (auto *UO = dyn_cast<UnaryOperator>(E)){
        if (UO->getOpcode() == UO_LNot)
          if (EB->Block == FalseOrDefaultBlock){
            E = IgnoreCasts(UO->getSubExpr());
            FillGenSet(E, BlockMap[pred], EB);
          }
      }else{
        if (EB->Block == FalseOrDefaultBlock)
          continue;
      // Check if the pred ends in a switch statement.
      const Stmt *TerminatorStmt = pred->getTerminatorStmt();
      if (TerminatorStmt && isa<SwitchStmt>(TerminatorStmt)) {

        // According to C11 standard section 6.8.4.2, the controlling
        // expression of a switch shall have integer type.
        // If we have switch(*p) where p is _Nt_array_ptr<char> then it is
        // casted to integer type and an IntegralCast is generated. Here we
        // strip off the IntegralCast.
        if (auto *CE = dyn_cast<CastExpr>(E)) {
          if (CE->getCastKind() == CastKind::CK_IntegralCast)
            E = CE->getSubExpr();
        }

        // If the current block has a null case label, we cannot widen the
        // bounds inside that case.
        if (IsSwitchCaseNull) {
          // If we are here it means that the current case label is null.
          // This means that the default case would represent the non-null
          // case. Hence, we can widen the bounds inside the default case.
          if (FalseOrDefaultBlock && FalseOrDefaultBlock->getLabel() &&
              isa<DefaultStmt>(FalseOrDefaultBlock->getLabel()))
            FillGenSet(E, BlockMap[pred], BlockMap[FalseOrDefaultBlock]);

          continue;
        }
      }
  }
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

  if (DeclRefExpr *D = GetDeclOperand(E))
    if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
      BoundsVars.insert(V);
}

DeclRefExpr *BoundsAnalysis::GetDeclOperand(const Expr *E) {
  if (auto *CE = dyn_cast_or_null<CastExpr>(E)) {
    const Expr *SubE = CE->getSubExpr();
    assert(SubE && "Invalid CastExpr expression");

    if (CE->getCastKind() == CastKind::CK_LValueToRValue ||
        CE->getCastKind() == CastKind::CK_ArrayToPointerDecay) {
      E = Lex.IgnoreValuePreservingOperations(Ctx, const_cast<Expr *>(SubE));
      return dyn_cast_or_null<DeclRefExpr>(const_cast<Expr *>(E));
    }
  }
  return nullptr;
}

void BoundsAnalysis::CollectNtPtrsInScope(FunctionDecl *FD) {
  // TODO: Currently, we simply collect all ntptrs and variables used in their
  // declared bounds for the entire function. Ultimately, we need to do a
  // liveness analysis of what ntptrs are in scope for a block.

  assert(FD && "invalid function");

  // Collect ntptrs passed as parameters to the current function.
  // Note: NtPtrsInScope is a mapping from ntptr to variables used in the
  // declared bounds of the ntptr. In this function we store an empty set for
  // the bounds variables. This will later be filled in FillGenSetForEdge.
  for (const ParmVarDecl *PD : FD->parameters()) {
    if (IsNtArrayType(PD))
      NtPtrsInScope[PD] = DeclSetTy();
  }

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
              NtPtrsInScope[V] = DeclSetTy();
      }
    }
  }
}

void BoundsAnalysis::FillGenSetForEdge(const Expr *E,
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

  for (auto item : NtPtrsInScope) {
    const VarDecl *V = item.first;

    BoundsExpr *NormalizedBounds = S.NormalizeBounds(V);
    if (!NormalizedBounds)
      continue;

    const auto *RBE = dyn_cast<RangeBoundsExpr>(NormalizedBounds);
    if (!RBE)
      continue;

    // Collect all variables involved in the upper and lower bounds exprs for
    // the ntptr. An assignment to any such variable would kill the widenend
    // bounds for the ntptr.
    DeclSetTy BoundsVars;
    CollectBoundsVars(NormalizedBounds, BoundsVars);
    NtPtrsInScope[V] = BoundsVars;

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

    if (!Lex.CompareExprSemantically(DerefBase, UpperBase))
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

    // Check if the difference overflows.
    bool Overflow;
    UpperOffset = DerefOffset.ssub_ov(UpperOffset, Overflow);
    if (Overflow)
      continue;

    EB->Gen[SuccEB->Block][V] = UpperOffset.getLimitedValue();

    // Top represents the union of the Gen sets of all edges. We have chosen
    // the offsets of ptr variables in Top to be the max unsigned int. The
    // reason behind this is that in order to compute the actual In sets for
    // blocks we are going to intersect the Out sets on all the incoming edges
    // of the block. And in that case we would always pick the ptr with the
    // smaller offset. Chosing max unsigned int also makes handling Top much
    // easier as we do not need to explicitly store edge info.
    Top[V] = std::numeric_limits<unsigned>::max();
  }
}

ExprIntPairTy BoundsAnalysis::SplitIntoBaseOffset(const Expr *E) {
  // In order to make an expression uniform, we want to keep all DeclRefExprs
  // on the LHS and all IntegerLiterals on the RHS.

  llvm::APSInt Zero (Ctx.getTypeSize(Ctx.IntTy), 0);

  if (!E)
    return std::make_pair(nullptr, Zero);

  E = E->IgnoreParens();

  if (DeclRefExpr *D = GetDeclOperand(E))
    return std::make_pair(D, Zero);

  if (!isa<BinaryOperator>(E))
    return std::make_pair(nullptr, Zero);

  const BinaryOperator *BO = dyn_cast<BinaryOperator>(E);
  // TODO: Currently we only handle exprs containing additive operations.
  if (BO->getOpcode() != BO_Add)
    return std::make_pair(nullptr, Zero);

  Expr *LHS = BO->getLHS()->IgnoreParens();
  Expr *RHS = BO->getRHS()->IgnoreParens();

  assert((LHS && RHS) && "invalid BinaryOperator expression");

  // Note: Assume p, q, r are DeclRefExprs and i, j are IntegerLiterals.

  // Case 1: LHS is DeclRefExpr and RHS is IntegerLiteral. This expr is already
  // uniform.
  // p + i ==> return (p, i)
  llvm::APSInt IntVal;

  if (DeclRefExpr *D = GetDeclOperand(LHS))
    if (RHS->isIntegerConstantExpr(IntVal, Ctx))
      return std::make_pair(D, IntVal);

  // Case 2: LHS is IntegerLiteral and RHS is DeclRefExpr. We simply need to
  // swap LHS and RHS to make expr uniform.
  // i + p ==> return (p, i)
  if (DeclRefExpr *D = GetDeclOperand(RHS))
    if (LHS->isIntegerConstantExpr(IntVal, Ctx))
      return std::make_pair(D, IntVal);

  // Case 3: LHS and RHS are both DeclRefExprs. This means there is no
  // IntegerLiteral in the expr. In this case, we return the incoming
  // BinaryOperator expr with a nullptr for the RHS.
  // p + q ==> return (p + q, nullptr)
  if (GetDeclOperand(LHS) && GetDeclOperand(RHS))
    return std::make_pair(BO, Zero);

  // To make parsing simpler, we always try to keep BinaryOperator on the LHS.
  if (!isa<BinaryOperator>(LHS) && isa<BinaryOperator>(RHS))
    std::swap(LHS, RHS);

  // By this time the LHS should be a BinaryOperator; either because it already
  // was a BinaryOperator, or because the RHS was a BinaryOperator and was
  // swapped with the LHS.
  if (!isa<BinaryOperator>(LHS))
    return std::make_pair(nullptr, Zero);

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

    // Since we are reassociating integers here, check if the value
    // overflows.
    bool Overflow;
    IntVal = IntVal.sadd_ov(BinOpRHS, Overflow);
    if (Overflow)
      return std::make_pair(nullptr, Zero);

    return std::make_pair(BinOpLHS, IntVal);
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

    FillGenSetForEdge(BO, EB, SuccEB);

  } else if (auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() == UO_Deref) {
      assert(UO->getSubExpr() && "invalid UnaryOperator expression");

      const Expr *UE = IgnoreCasts(UO->getSubExpr());
      FillGenSetForEdge(UE, EB, SuccEB);
    }
  }
}

void BoundsAnalysis::ComputeKillSets(StmtSet NestedStmts) {
  // For a block B, a variable V is added to Kill[B][S] if V is assigned to in
  // B by Stmt S or some child S1 of S.

  // Compute vars killed in the current block.
  for (const auto item : BlockMap) {
    ElevatedCFGBlock *EB = item.second;

    for (CFGElement Elem : *(EB->Block)) {
      if (Elem.getKind() == CFGElement::Statement) {
        const Stmt *S = Elem.castAs<CFGStmt>().getStmt();
        if (!S)
          continue;

        // Skip top-level statements that are nested in another
        // top-level statement.
        if (NestedStmts.find(S) != NestedStmts.end())
          continue;

        FillKillSet(EB, S, S);
      }
    }
  }
}

void BoundsAnalysis::FillKillSet(ElevatedCFGBlock *EB,
                                 const Stmt *TopLevelStmt,
                                 const Stmt *S) {
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

        // If the variable being assigned to is an ntptr, add the Stmt:V pair
        // to the Kill set for the block.
        if (IsNtArrayType(V))
          EB->Kill[TopLevelStmt].insert(V);

        else {
          // Else look for the variable in NtPtrsInScope.

          // NtPtrsInScope is a mapping from an ntptr to all the variables used
          // in its upper and lower bounds exprs. For example:

          // _Nt_array_ptr<char> p : bounds(p + i, i + p + j + 10);
          // _Nt_array_ptr<char> q : bounds(i + q, i + p + q + m);

          // NtPtrsInScope: {p: {p, i, j}, q: {i, q, p, m}}

          for (auto item : NtPtrsInScope) {
            const VarDecl *NtPtr = item.first;
            DeclSetTy BoundsVars = item.second;

            // If the variable exists in the bounds declaration for the ntptr,
            // then add the Stmt:ntptr pair to the Kill set for the block.
            if (BoundsVars.count(V))
              EB->Kill[TopLevelStmt].insert(NtPtr);
          }
        }
      }
    }
  }

  for (const Stmt *St : S->children())
    FillKillSet(EB, TopLevelStmt, St);
}

void BoundsAnalysis::ComputeInSets(ElevatedCFGBlock *EB) {
  // In[B1] = n Out[B*->B1], where B* are all preds of B1.

  BoundsMapTy Intersections;
  bool FirstIntersection = true;

  for (const CFGBlock *pred : EB->Block->preds()) {
    if (SkipBlock(pred))
      return;

    ElevatedCFGBlock *PredEB = BlockMap[pred];

    if (FirstIntersection) {
      Intersections = PredEB->Out[EB->Block];
      FirstIntersection = false;
    } else
      Intersections = Intersect(Intersections, PredEB->Out[EB->Block]);
  }

  EB->In = Intersections;
}

void BoundsAnalysis::ComputeOutSets(ElevatedCFGBlock *EB,
                                    WorkListTy &WorkList) {
  // Out[B1->B2] = (In[B1] - Kill[B1]) u Gen[B1->B2].

  // EB->Kill is a mapping from Stmt to ntptrs. We extract just the ntptrs
  // killed for the block and use that to compute (In - Kill).
  DeclSetTy KilledVars;
  for (auto item : EB->Kill) {
    const DeclSetTy Vars = item.second;
    KilledVars.insert(Vars.begin(), Vars.end());
  }

  BoundsMapTy InMinusKill = Difference(EB->In, KilledVars);

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

    EB->Out[succ] = Union(InMinusKill, EB->Gen[succ]);

    // The Out set on an edge is marked "empty" if the In set is marked "empty"
    // and the Gen set on that edge is empty.
    EB->IsOutSetEmpty[succ] = EB->IsInSetEmpty && !EB->Gen[succ].size();

    if (Differ(OldOut, EB->Out[succ]))
      WorkList.append(BlockMap[succ]);
  }
}

StmtDeclSetTy BoundsAnalysis::GetKilledBounds(const CFGBlock *B) {
  auto I = BlockMap.find(B);
  if (I == BlockMap.end())
    return StmtDeclSetTy();

  ElevatedCFGBlock *EB = I->second;
  return EB->Kill;
}

BoundsMapTy BoundsAnalysis::GetWidenedBounds(const CFGBlock *B) {
  auto I = BlockMap.find(B);
  if (I == BlockMap.end())
    return BoundsMapTy();

  ElevatedCFGBlock *EB = I->second;
  return EB->In;
}

Expr *BoundsAnalysis::GetTerminatorCondition(const Expr *E) const {
  if (const auto *BO = dyn_cast<BinaryOperator>(E->IgnoreParens()))
    return GetTerminatorCondition(BO->getRHS());

  // According to C11 standard section 6.5.13, the logical AND Operator
  // shall yield 1 if both of its operands compare unequal to 0;
  // otherwise, it yields 0. The result has type int.
  // If we have if (*p && *(p + 1)) where p is _Nt_array_ptr<char> then
  // it is casted to integer type and an IntegralCast is generated. Here
  // we strip off the IntegralCast.
  if (auto *CE = dyn_cast<CastExpr>(E))
    if (CE->getCastKind() == CastKind::CK_IntegralCast)
      return const_cast<Expr *>(CE->getSubExpr());
  return const_cast<Expr *>(E);
}

Expr *BoundsAnalysis::GetTerminatorCondition(const CFGBlock *B) const {
  if (const Stmt *S = B->getTerminatorStmt()) {
    if (const auto *BO = dyn_cast<BinaryOperator>(S))
      return GetTerminatorCondition(BO->getLHS());
    if (const auto *IfS = dyn_cast<IfStmt>(S))
      return GetTerminatorCondition(IfS->getCond());
    if (const auto *WhileS = dyn_cast<WhileStmt>(S))
      return const_cast<Expr *>(WhileS->getCond());
    if (const auto *ForS = dyn_cast<ForStmt>(S))
      return const_cast<Expr *>(ForS->getCond());
    if (const auto *SwitchS = dyn_cast<SwitchStmt>(S))
      return const_cast<Expr *>(SwitchS->getCond());
  }
  return nullptr;
}

Expr *BoundsAnalysis::IgnoreCasts(const Expr *E) {
  return Lex.IgnoreValuePreservingOperations(Ctx, const_cast<Expr *>(E));
}

bool BoundsAnalysis::IsNtArrayType(const VarDecl *V) const {
  return V && (V->getType()->isCheckedPointerNtArrayType() ||
               V->getType()->isNtCheckedArrayType());
}

bool BoundsAnalysis::SkipBlock(const CFGBlock *B) const {
  return !B || B == &Cfg->getExit();
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
  for (auto I : A) {
    const auto *V = I.first;
    if (B.count(V))
      Ret.erase(V);
  }
  return Ret;
}

template<class T>
bool BoundsAnalysis::Differ(T &A, T &B) const {
  if (A.size() != B.size())
    return true;

  for (const auto item : A) {
    if (!B.count(item.first))
      return true;
    if (B[item.first] != item.second)
      return true;
  }

  for (const auto item : B) {
    if (!A.count(item.first))
      return true;
    if (A[item.first] != item.second)
      return true;
  }
  return false;
}

OrderedBlocksTy BoundsAnalysis::GetOrderedBlocks() {
  // WidenedBounds is a DenseMap and hence is not suitable for iteration as its
  // iteration order is non-deterministic. So we first need to order the
  // blocks. The block IDs decrease from entry to exit. So we sort in the
  // reverse order.
  OrderedBlocksTy OrderedBlocks;
  for (auto item : BlockMap) {
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
  OS << "--------------------------------------\n";
  OS << "In function: " << FD->getName() << "\n";

  for (const CFGBlock *B : GetOrderedBlocks()) {
    OS << "--------------------------------------";
    B->print(OS, Cfg, S.getLangOpts(), /* ShowColors */ true);

    BoundsMapTy Vars = GetWidenedBounds(B);
    using VarPairTy = std::pair<const VarDecl *, unsigned>;

    llvm::sort(Vars.begin(), Vars.end(),
               [](VarPairTy A, VarPairTy B) {
                 return A.first->getQualifiedNameAsString().compare(
                        B.first->getQualifiedNameAsString()) < 0;
               });

    for (auto item : Vars) {
      OS << "upper_bound("
         << item.first->getQualifiedNameAsString() << ") = "
         << item.second << "\n";
    }
  }
}

} // end namespace clang
