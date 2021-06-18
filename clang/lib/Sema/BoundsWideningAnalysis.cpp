//===== BoundsWideningAnalysis.h - Dataflow analysis for bounds widening ====//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// This file implements a dataflow analysis for bounds widening as described in
// https://github.com/microsoft/checkedc-clang/blob/master/clang/docs/checkedc/Bounds-Widening-for-Null-Terminated-Arrays.md
//===---------------------------------------------------------------------===//

#include "clang/Sema/BoundsWideningAnalysis.h"

namespace clang {

//===---------------------------------------------------------------------===//
// Implementation of the methods in the BoundsWideningAnalysis class. This is
// the main class that implements the dataflow analysis for bounds widening of
// null-terminated arrays. This class uses helper methods from the
// BoundsWideningUtil class that are defined later in this file.
//===---------------------------------------------------------------------===//

void BoundsWideningAnalysis::WidenBounds(FunctionDecl *FD) {
  assert(Cfg && "expected CFG to exist");

  // Initialize the list of variables that are pointers to null-terminated
  // arrays to the null-terminated arrays that are passed as parameters to the
  // function.
  InitNtPtrsInFunc(FD);

  WorkListTy WorkList;

  // Add each block to WorkList and create a mapping from CFGBlock to
  // ElevatedCFGBlock.
  // Note: By default, PostOrderCFGView iterates in reverse order. So we always
  // get a reverse post order when we iterate PostOrderCFGView.
  for (const CFGBlock *B : PostOrderCFGView(Cfg)) {
    // SkipBlock will skip all null blocks and the exit block. PostOrderCFGView
    // does not traverse any unreachable blocks. So at the end of this loop
    // BlockMap only contains reachable blocks.
    if (BWUtil.SkipBlock(B))
      continue;

    auto EB = new ElevatedCFGBlock(B);
    BlockMap[B] = EB;

    // Note: WorkList is a queue. So we maintain the reverse post order when we
    // iterate WorkList.
    WorkList.append(EB);

    // Compute Gen and Kill sets for the block and statements in the block.
    ComputeGenKillSets(EB);

    // Initialize the In and Out sets for the block.
    InitBlockInOutSets(FD, EB);
  }

  // Compute the In and Out sets for blocks.
  while (!WorkList.empty()) {
    ElevatedCFGBlock *EB = WorkList.next();
    WorkList.remove(EB);

    ComputeInSet(EB);
    ComputeOutSet(EB, WorkList);
  }
}

void BoundsWideningAnalysis::ComputeGenKillSets(ElevatedCFGBlock *EB) {
  const Stmt *PrevStmt = nullptr;

  for (CFGBlock::const_iterator I = EB->Block->begin(),
                                E = EB->Block->end();
       I != E; ++I) {
    CFGElement Elem = *I;
    if (Elem.getKind() == CFGElement::Statement) {
      const Stmt *CurrStmt = Elem.castAs<CFGStmt>().getStmt();
      if (!CurrStmt)
        continue;

      ComputeStmtGenKillSets(EB, CurrStmt);
      ComputeUnionGenKillSets(EB, CurrStmt, PrevStmt);
      UpdateNtPtrsInFunc(EB, CurrStmt);

      EB->PrevStmtMap[CurrStmt] = PrevStmt;
      PrevStmt = CurrStmt;

      // Save the last statement of the block.
      if (I == E - 1)
        EB->LastStmt = CurrStmt;
    }
  }

  ComputeBlockGenKillSets(EB);
}

void BoundsWideningAnalysis::ComputeStmtGenKillSets(ElevatedCFGBlock *EB,
                                                    const Stmt *CurrStmt) {
  // Initialize the Gen and Kill sets for the statement.
  EB->StmtGen[CurrStmt] = BoundsMapTy();
  EB->StmtKill[CurrStmt] = VarSetTy();

  BoundsMapTy VarsAndBounds;

  // Determine whether CurrStmt generates a dataflow fact.

  // A conditional statement that dereferences a variable that is a pointer to
  // a null-terminated array can generate a dataflow fact. For example: if (*(p
  // + 1)) The conditional will always be the terminator statement of the block.
  if (CurrStmt == EB->Block->getLastCondition()) {
    GetVarsAndBoundsInPtrDeref(EB, VarsAndBounds);

  // A bounds declaration of a null-terminated array generates a dataflow fact.
  // For example: _Nt_array_ptr<char> p : bounds(p, p + 1);
  } else if (const auto *DS = dyn_cast<DeclStmt>(CurrStmt)) {
    for (const Decl *D : DS->decls()) {
      if (const auto *V = dyn_cast<VarDecl>(D)) {
        if (!V->isInvalidDecl()) {
          GetVarsAndBoundsInDecl(EB, V, VarsAndBounds);

          // Additionally, a where clause on a declaration can generate a
          // dataflow fact.
          // For example: int x = strlen(p) _Where p : bounds(p, p + x);
          GetVarsAndBoundsInWhereClause(EB, V->getWhereClause(), VarsAndBounds);
        }
      }
    }

  // A where clause on an expression statement (which is represented in the
  // AST as a ValueStmt) can generate a dataflow fact.
  // For example: x = strlen(p) _Where p : bounds(p, p + x);
  } else if (const auto *VS = dyn_cast<ValueStmt>(CurrStmt)) {
    GetVarsAndBoundsInWhereClause(EB, VS->getWhereClause(), VarsAndBounds);

  // A where clause on a null statement (meaning a standalone where clause) can
  // generate a dataflow fact.
  // For example: _Where p : bounds(p, p + 1);
  // TODO: Currently, a null statement does not occur in the list of
  // statements of a block. As a result, there are no Gen and Kill sets for a
  // null statement. So currently the above example does not generate a
  // dataflow fact.
  } else if (const auto *NS = dyn_cast<NullStmt>(CurrStmt)) {
    GetVarsAndBoundsInWhereClause(EB, NS->getWhereClause(), VarsAndBounds);
  }

  // Using the mapping of variables to bounds expressions in VarsAndBounds fill
  // the Gen and Kill sets for the statement.
  FillStmtGenKillSets(EB, CurrStmt, VarsAndBounds);

  // If a variable modified by CurrStmt occurs in the bounds expression of a
  // null-terminated array then the bounds of that null-terminated array should
  // be killed.
  AddModifiedVarsToStmtKillSet(EB, CurrStmt);
}

void BoundsWideningAnalysis::ComputeUnionGenKillSets(ElevatedCFGBlock *EB,
                                                     const Stmt *CurrStmt,
                                                     const Stmt *PrevStmt) {
  // If this is the first statement in the block.
  if (!PrevStmt) {
    EB->UnionGen[CurrStmt] = EB->StmtGen[CurrStmt];
    EB->UnionKill[CurrStmt] = EB->StmtKill[CurrStmt];
    return;
  }

  EB->UnionKill[CurrStmt] = BWUtil.Union(EB->UnionKill[PrevStmt],
                                         EB->StmtKill[CurrStmt]);

  auto Diff = BWUtil.Difference(EB->UnionGen[PrevStmt],
                                EB->StmtKill[CurrStmt]);
  EB->UnionGen[CurrStmt] = BWUtil.Union(Diff, EB->StmtGen[CurrStmt]);
}

void BoundsWideningAnalysis::ComputeBlockGenKillSets(ElevatedCFGBlock *EB) {
  // Initialize the Gen and Kill sets for the block.
  EB->Gen = BoundsMapTy();
  EB->Kill = VarSetTy();

  if (EB->LastStmt) {
    EB->Gen = EB->UnionGen[EB->LastStmt];
    EB->Kill = EB->UnionKill[EB->LastStmt];
  }
}

void BoundsWideningAnalysis::ComputeInSet(ElevatedCFGBlock *EB) {
  const CFGBlock *CurrBlock = EB->Block;

  // Iterate through all the predecessor blocks of EB.
  for (const CFGBlock *PredBlock : CurrBlock->preds()) {
    auto BlockIt = BlockMap.find(PredBlock);
    if (BlockIt == BlockMap.end())
      continue;

    ElevatedCFGBlock *PredEB = BlockIt->second;
    BoundsMapTy PrunedOutSet = PruneOutSet(PredEB, EB);

    EB->In = BWUtil.Intersect(EB->In, PrunedOutSet);
  }
}

BoundsMapTy BoundsWideningAnalysis::PruneOutSet(
  ElevatedCFGBlock *PredEB, ElevatedCFGBlock *CurrEB) const {

  const CFGBlock *PredBlock = PredEB->Block;
  const CFGBlock *CurrBlock = CurrEB->Block;

  // If the edge from pred to the current block is a fallthrough edge then the
  // Out of the pred should simply "flow" through to the current block (meaning
  // PredOut should simply be intersected with the In of the current block).
  // Fallthrough edges are generated in case of loops. For example:

  // _Nt_array_ptr<char> p : bounds(p, p + 1);
  // while (*(p + 1))
  //   a = 1;

  // B1: pred: Entry, succ: B2
  //   _Nt_array_ptr<char> p : bounds(p, p + 1);

  // B2: pred: B1, B4, succ: B3

  // B3: pred: B2, succ: B4
  //   while (*(p + 1))

  // B4: pred, B3, succ: B2
  //   a = 1;

  // In the above example, the edges B1->B2, B2->B3 and B4->B2 are fallthrough
  // edges. So in this case, the Out sets of the pred blocks should simply flow
  // through to the successor blocks. Meaning we do not need to prune the Out
  // set of the pred block.

  if (BWUtil.IsFallthroughEdge(PredBlock, CurrBlock))
    return PredEB->Out;

  BoundsMapTy PrunedOutSet = PredEB->Out;

  // Check if the edge from pred to the current block is a true edge.
  bool IsEdgeTrue = BWUtil.IsTrueEdge(PredBlock, CurrBlock);

  // Get the StmtIn of the last statement in the pred block. If the pred
  // block does not have any statements then StmtInOfLastStmtOfPred is set to
  // the In set of the pred block.
  BoundsMapTy StmtInOfLastStmtOfPred = GetStmtIn(PredBlock, PredEB->LastStmt);

  // Check if the current block is a case of a switch-case.
  bool IsSwitchCase = BWUtil.IsSwitchCaseBlock(CurrBlock);

  // If the current block is a case of a switch-case, check if the case label
  // tests for null. CaseLabelTestsForNull returns false for the default case.
  bool IsCaseLabelNull = IsSwitchCase &&
                         BWUtil.CaseLabelTestsForNull(CurrBlock);

  // PredEB->Out may contain the widened bounds "E + 1" for some variable V.
  // Here, we need to determine if "E + 1" should make it to the In set of
  // the current block. To determine this, we need to check if the
  // dereference is at the upper bound and if we are on a true edge. Else we
  // will reset the bounds of V in PrunedOutSet to the bounds of V in
  // StmtInOfLastStmtOfPred.
  for (auto VarBoundsPair : PredEB->Out) {
    const VarDecl *V = VarBoundsPair.first;
    auto StmtInIt = StmtInOfLastStmtOfPred.find(V);

    // If V is not present in StmtIn of the last statement (maybe as a result
    // of being killed before the last statement) then V cannot be widened in
    // this block. So we remove V from the computation of the In set for this
    // block. For example:

    // B1:
    //   1: _Nt_array_ptr<char> p : bounds(p, p + i);
    //   2: i = 0;
    //   3: if (*(p + i)) {
    // B2:
    //   4: a = 1;
    //   }

    // The bounds of p are killed at statement 2. So StmtIn[3] does not
    // contain p but Gen[3] will contain "p : bounds(p, p + i + 1)". So we
    // need to remove p from the In set of successor block.

    // Note: We could have done this as part of the Intersect function but
    // that would have meant teaching Intersect about StmtIn, etc which would
    // have complicated the logic for intersection. So to keep the Intersect
    // method simple we check if V is in StmtIn of the last statement of pred
    // and accordingly remove it from PrunedOutSet.

    if (StmtInIt == StmtInOfLastStmtOfPred.end()) {
      PrunedOutSet.erase(V);
      continue;
    }

    RangeBoundsExpr *BoundsInStmtIn = StmtInIt->second;

    // If the bounds of V before the last statement in the pred block is Top
    // then we retain whatever the bounds of V are in PrunedOutSet and they
    // will be part of the computation of In set of the current block.
    if (BoundsInStmtIn == Top)
      continue;

    // If the edge from pred to the current block is not a true edge then we
    // cannot widen the bounds upon entry to the current block. So we reset
    // the bounds to those before the last statement in the pred block.
    // For example:

    // Block B1:
    //   if (*(p + 1)) {
    // Block B2:
    //   } else {
    // Block B3:
    //   }

    // The edge B1->B2 is a true edge for the condition "if (*(p + 1))". So
    // we can widen the bounds upon entry to B2.
    // The edge B1->B3 is a false edge for the condition "if (*(p + 1))". So
    // we cannot widen the bounds upon entry to B3.

    // Note: Switch cases are handled separately later in this function.

    if (!IsSwitchCase && !IsEdgeTrue) {
      PrunedOutSet[V] = BoundsInStmtIn;
      continue;
    }

    // If the terminating condition of the pred block does not
    // dereference V at the current upper bound, then we cannot widen the
    // bounds upon entry to the current block. So we reset the bounds of V to
    // those before the last statement in pred. For example:

    // Block 1:
    //   1: _Nt_array_ptr<char> p : bounds(p, p + 1);
    //   2: if (*(p)) {
    // Block 2:
    //   }
    //   3: if (*(p + 2)) {
    // Block 3:
    //   }

    // In the example above, the conditionals at statements 2 and 3 do not
    // dereference p at its upper bound. So we cannot widen the bounds of p
    // upon entry to blocks 2 and 3.

    bool IsDerefAtUpperBound =
      PredEB->TermCondDerefExpr &&
      Lex.CompareExprSemantically(PredEB->TermCondDerefExpr,
                                  BoundsInStmtIn->getUpperExpr());

    if (!IsDerefAtUpperBound) {
      PrunedOutSet[V] = BoundsInStmtIn;
      continue;
    }

    // If we are here then it means that the terminating condition of the
    // pred block dereferences V at its current upper bound.

    // If we are in a block that is a case of a switch-case.
    if (IsSwitchCase) {
      // We cannot widen the bounds in the following scenarios:

      // 1. The case label tests for null. For example:

      // _Nt_array_ptr<char> p : bounds(p, p + 1);
      // switch(*(p + 1)) {
      // case '\0': // cannot widen here.
      // }

      if (IsCaseLabelNull)
        PrunedOutSet[V] = BoundsInStmtIn;

      // 2. We are in a default case and there is no other case that tests
      // for null. For example:

      // switch(*(p + 1)) {
      // default: can widen here.
      // case '\0': cannot widen here.
      // case 'a': can widen here.
      // }

      // switch(*(p + 1)) {
      // default: cannot widen here.
      // case 'a': can widen here.
      // case 'b': can widen here.
      // }

      else if (isa<DefaultStmt>(CurrBlock->getLabel()) &&
              !BWUtil.ExistsNullCaseLabel(PredBlock))
        PrunedOutSet[V] = BoundsInStmtIn;
    }
  }

  return PrunedOutSet;
}

void BoundsWideningAnalysis::ComputeOutSet(ElevatedCFGBlock *EB,
                                           WorkListTy &WorkList) {
  auto OrigOut = EB->Out;

  auto Diff = BWUtil.Difference(EB->In, EB->Kill);
  EB->Out = BWUtil.Union(Diff, EB->Gen);

  // If the Out set of the current block has changed add the current block to
  // the WorkList.
  if (!BWUtil.IsEqual(EB->Out, OrigOut)) {
    WorkList.append(EB);

    // Also add all successors of the current block to the WorkList.
    for (const CFGBlock *SuccBlock : EB->Block->succs()) {
      if (!BWUtil.SkipBlock(SuccBlock))
        WorkList.append(BlockMap[SuccBlock]);
    }
  }
}

void BoundsWideningAnalysis::InitBlockInOutSets(FunctionDecl *FD,
                                                ElevatedCFGBlock *EB) {
  // Initialize the In and Out sets for the entry block.
  if (EB->Block == &Cfg->getEntry()) {
    EB->In = BoundsMapTy();
    EB->Out = BoundsMapTy();

    // If the function has parameters that generate dataflow facts then add
    // those to the In and Out sets of the entry block.
    BoundsMapTy VarsAndBounds;
    for (const ParmVarDecl *PD : FD->parameters()) {
      // Bounds declarations on a function parameter can generate a dataflow
      // fact.
      // For example: void foo(_Nt_array_ptr<char> p : bounds(p, p + 1)) {}
      GetVarsAndBoundsInDecl(EB, PD, VarsAndBounds);

      // Additionally, a where clause on a function parameter can generate a
      // dataflow fact.
      // For example: void foo(int x _Where p : bounds(p, p + 1)) {}
      GetVarsAndBoundsInWhereClause(EB, PD->getWhereClause(), VarsAndBounds);
    }

    for (auto VarBoundsPair : VarsAndBounds) {
      const VarDecl *V = VarBoundsPair.first;
      RangeBoundsExpr *R = VarBoundsPair.second;

      EB->In[V] = R;
      EB->Out[V] = R;
    }

  } else {
    // Initialize the In and Out sets for the rest of the blocks to Top.
    for (const VarDecl *V : AllNtPtrsInFunc) {
      EB->In[V] = Top;
      EB->Out[V] = Top;
    }
  }
}

BoundsMapTy BoundsWideningAnalysis::GetStmtOut(const CFGBlock *B,
                                               const Stmt *CurrStmt) const {
  // Note: This method can be called from outside the BoundsWideningAnalysis
  // class to get the widened bounds after a statement.
  if (!B)
    return BoundsMapTy();

  auto BlockIt = BlockMap.find(B);
  if (BlockIt == BlockMap.end())
    return BoundsMapTy();

  ElevatedCFGBlock *EB = BlockIt->second;

  if (CurrStmt) {
    auto Diff = BWUtil.Difference(EB->In, EB->UnionKill[CurrStmt]);
    return BWUtil.Union(Diff, EB->UnionGen[CurrStmt]);
  }
  return EB->In;
}

BoundsMapTy BoundsWideningAnalysis::GetStmtIn(const CFGBlock *B,
                                              const Stmt *CurrStmt) const {
  // Note: This method can be called from outside the BoundsWideningAnalysis
  // class to get the widened bounds before a statement.
  if (!B)
    return BoundsMapTy();

  auto BlockIt = BlockMap.find(B);
  if (BlockIt == BlockMap.end())
    return BoundsMapTy();

  ElevatedCFGBlock *EB = BlockIt->second;

  // StmtIn of a statement is equal to the StmtOut of its previous statement.
  return GetStmtOut(B, EB->PrevStmtMap[CurrStmt]);
}

void BoundsWideningAnalysis::InitNtPtrsInFunc(FunctionDecl *FD) {
  // Initialize the list of variables that are pointers to null-terminated
  // arrays to the null-terminated arrays that are passed as parameters to the
  // function.
  for (const ParmVarDecl *PD : FD->parameters()) {
    if (BWUtil.IsNtArrayType(PD))
      AllNtPtrsInFunc.insert(PD);
  }
}

void BoundsWideningAnalysis::UpdateNtPtrsInFunc(ElevatedCFGBlock *EB,
                                                const Stmt *CurrStmt) {
  // Add variables occurring in StmtGen for the current statement to the list
  // of variables that are pointers to null-terminated arrays.
  for (auto VarBoundsPair : EB->StmtGen[CurrStmt])
    AllNtPtrsInFunc.insert(VarBoundsPair.first);
}

void BoundsWideningAnalysis::FillStmtGenKillSets(ElevatedCFGBlock *EB,
                                                 const Stmt *CurrStmt,
                                                 BoundsMapTy &VarsAndBounds) {
  for (auto VarBoundsPair : VarsAndBounds) {
    const VarDecl *V = VarBoundsPair.first;
    RangeBoundsExpr *R = VarBoundsPair.second;

    EB->StmtGen[CurrStmt][V] = R;
    EB->StmtKill[CurrStmt].insert(V);
  }
}

void BoundsWideningAnalysis::GetVarsAndBoundsInDecl(
  ElevatedCFGBlock *EB, const VarDecl *V, BoundsMapTy &VarsAndBounds) {

  if (BWUtil.IsNtArrayType(V)) {
    BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(V);
    VarsAndBounds[V] = dyn_cast<RangeBoundsExpr>(NormalizedBounds);
  }
}

void BoundsWideningAnalysis::GetVarsAndBoundsInWhereClause(
  ElevatedCFGBlock *EB, WhereClause *WC, BoundsMapTy &VarsAndBounds) {

  if (!WC)
    return;

  for (const auto *Fact : WC->getFacts()) {
    auto *BF = dyn_cast<BoundsDeclFact>(Fact);
    if (!BF)
      continue;

    VarDecl *V = BF->Var;
    if (BWUtil.IsNtArrayType(V)) {
      BoundsExpr *NormalizedBounds = SemaRef.ExpandBoundsToRange(V, BF->Bounds);
      VarsAndBounds[V] = dyn_cast<RangeBoundsExpr>(NormalizedBounds);
    }
  }
}

void BoundsWideningAnalysis::GetVarsAndBoundsInPtrDeref(
  ElevatedCFGBlock *EB, BoundsMapTy &VarsAndBounds) {

  // Get the terminating condition of the block.
  // If we have a terminating condition like "if (e1 && e2 && e3)" then 3
  // blocks would be created for e1, e2 and e3 each. getTerminatorStmt() would
  // return the entire condition "e1 && e2 && e3" but getLastCondition()
  // returns only "e3" which is what we need.
  const Expr *TermCond = EB->Block->getLastCondition();
  if (!TermCond)
    return;

  // If the terminating condition is a dereference expression get that
  // expression. For example, from a terminating condition like "if (*(p +
  // 1))", extract the expression "p + 1".
  Expr *DerefExpr = BWUtil.GetDerefExpr(TermCond);
  if (!DerefExpr)
    return;

  EB->TermCondDerefExpr = DerefExpr;

  // Get all variables in the expression that are pointers to null-terminated
  // arrays. For example: On a dereference expression like "*(p + i + j + 1)"
  // GetNtPtrsInExpr() will return {p} if p is a pointer to a null-terminated
  // array.

  VarSetTy NtPtrsInExpr;
  BWUtil.GetNtPtrsInExpr(DerefExpr, NtPtrsInExpr);

  // Get all variables that are pointers to null-terminated arrays and in whose
  // upper bounds expressions each of the variables in NtPtrsInExpr occur. For
  // example:

  // _Nt_array_ptr<char> p : bounds(p, p);
  // _Nt_array_ptr<char> q : bounds(q, p + i);
  // _Nt_array_ptr<char> r : bounds(r, p + 2);
  // _Nt_array_ptr<char> s : bounds(p, s);

  // On a dereference expression like "*(p + i + j + 1)"
  // GetNtPtrsWithVarsInUpperBounds() will return {p, q, r} because p occurs in
  // the upper bounds expressions of p, q and r.

  VarSetTy NtPtrsWithVarsInUpperBounds;
  BWUtil.GetNtPtrsWithVarsInUpperBounds(NtPtrsInExpr,
                                        NtPtrsWithVarsInUpperBounds);

  // Now, the bounds of all variables in NtPtrsWithVarsInUpperBounds can
  // potentially be widened to bounds(lower, DerefExpr + 1).

  for (const VarDecl *V : NtPtrsWithVarsInUpperBounds) {
    BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(V);
    RangeBoundsExpr *RBE = dyn_cast<RangeBoundsExpr>(NormalizedBounds);

    // DerefExpr potentially widens V. So we need to add "V:bounds(lower,
    // DerefExpr + 1)" to the Gen set.

    // TODO: Here, we always consider the lower bound from the declared bounds
    // of V. But this may be incorrect in case a where clause redeclares the
    // lower bound of V. This needs to be handled.

    RangeBoundsExpr *WidenedBounds =
      new (Ctx) RangeBoundsExpr(RBE->getLowerExpr(),
                                BWUtil.AddOffsetToExpr(DerefExpr, 1),
                                SourceLocation(), SourceLocation());
    VarsAndBounds[V] = WidenedBounds;
  }
}

void BoundsWideningAnalysis::AddModifiedVarsToStmtKillSet(
  ElevatedCFGBlock *EB, const Stmt *CurrStmt) {

  // Get variables modified by CurrStmt or statements nested in CurrStmt.
  VarSetTy ModifiedVars;
  BWUtil.GetModifiedVars(CurrStmt, ModifiedVars);

  // Get the set of variables that are pointers to null-terminated arrays and
  // in whose lower and upper bounds expressions the modified variables occur.
  VarSetTy NtPtrsWithVarsInBounds;
  BWUtil.GetNtPtrsWithVarsInLowerBounds(ModifiedVars, NtPtrsWithVarsInBounds);
  BWUtil.GetNtPtrsWithVarsInUpperBounds(ModifiedVars, NtPtrsWithVarsInBounds);

  for (const VarDecl *V : NtPtrsWithVarsInBounds)
    EB->StmtKill[CurrStmt].insert(V);
}

void BoundsWideningAnalysis::DumpWidenedBounds(FunctionDecl *FD) {
  OS << "\n--------------------------------------";
  // Print the function name.
  OS << "\nFunction: " << FD->getName();

  for (const CFGBlock *CurrBlock : GetOrderedBlocks()) {
    // Print the current block number.
    OS << "\nBlock: B" << CurrBlock->getBlockID();

    // Print the predecessor blocks of the current block.
    OS << ", Pred: ";
    for (const CFGBlock *PredBlock : CurrBlock->preds()) {
      if (PredBlock)
        OS << "B" << PredBlock->getBlockID() << ", ";
    }

    // Print the successor blocks of the current block.
    OS << "Succ: ";
    for (const CFGBlock *SuccBlock : CurrBlock->succs()) {
      if (SuccBlock) {
        OS << "B" << SuccBlock->getBlockID();

        if (SuccBlock != *(CurrBlock->succs().end() - 1))
          OS << ", ";
        }
    }

    bool IsBlockEmpty = true;
    for (CFGElement Elem : *CurrBlock) {
      if (Elem.getKind() == CFGElement::Statement) {
        const Stmt *CurrStmt = Elem.castAs<CFGStmt>().getStmt();
        if (!CurrStmt)
          continue;

        BoundsMapTy WidenedBounds = GetStmtIn(CurrBlock, CurrStmt);

        std::vector<const VarDecl *> Vars;
        for (auto VarBoundsPair : WidenedBounds)
          Vars.push_back(VarBoundsPair.first);

        llvm::sort(Vars.begin(), Vars.end(),
          [](const VarDecl *A, const VarDecl *B) {
             return A->getQualifiedNameAsString().compare(
                    B->getQualifiedNameAsString()) < 0;
          });

        std::string Str;
        llvm::raw_string_ostream SS(Str);
        CurrStmt->printPretty(SS, nullptr, Ctx.getPrintingPolicy());

        // Print the current statement.
        OS << "\n  Widened bounds before stmt: " << SS.str();
        if (SS.str().back() != '\n')
          OS << "\n";

        IsBlockEmpty = false;

        if (Vars.size() == 0)
          OS << "    <none>\n";

        for (const VarDecl *V : Vars) {
          RangeBoundsExpr *Bounds = WidenedBounds[V];
          if (Bounds == Top) {
            // If this is the only variable and its bounds are Top, we need to
            // print <none>.
            if (Vars.size() == 1)
              OS << "    <none>\n";
            continue;
          }

          Expr *Lower = Bounds->getLowerExpr();
          Expr *Upper = Bounds->getUpperExpr();

          OS << "    " << V->getQualifiedNameAsString() << ": bounds(";
          Lower->printPretty(OS, nullptr, Ctx.getPrintingPolicy());
          OS << ", ";
          Upper->printPretty(OS, nullptr, Ctx.getPrintingPolicy());
          OS << ")\n";
        }
      }
    }

    if (IsBlockEmpty)
      OS << "\n";
  }
}

OrderedBlocksTy BoundsWideningAnalysis::GetOrderedBlocks() const {
  // We order the CFG blocks based on block ID. Block IDs decrease from entry
  // to exit. So we sort in the reverse order.
  OrderedBlocksTy OrderedBlocks;
  for (auto BlockEBPair : BlockMap) {
    const CFGBlock *B = BlockEBPair.first;
    OrderedBlocks.push_back(B);
  }

  llvm::sort(OrderedBlocks.begin(), OrderedBlocks.end(),
    [] (const CFGBlock *A, const CFGBlock *B) {
        return A->getBlockID() > B->getBlockID();
    });
  return OrderedBlocks;
}
// end of methods for the BoundsWideningAnalysis class.

//===---------------------------------------------------------------------===//
// Implementation of the methods in the BoundsWideningUtil class. This class
// contains helper methods that are used by the BoundsWideningAnalysis class to
// perform the dataflow analysis.
//===---------------------------------------------------------------------===//

bool BoundsWideningUtil::IsSubRange(RangeBoundsExpr *B1,
                                    RangeBoundsExpr *B2) const {
  // If B2 is a subrange of B1, then
  // B2.Lower >= B1.Lower and B2.Upper <= B1.Upper

  // Examples:
  // B1 = bounds(p, p + 5) and B2 = bounds(p + 1, p + 3) ==> True
  // B1 = bounds(p, p + 5) and B2 = bounds(p, p + 5) ==> True
  // B1 = bounds(p, p + 5) and B2 = bounds(p, p + 2) ==> True
  // B1 = bounds(p, p + 5) and B2 = bounds(p + 4, p + 5) ==> True
  // B1 = bounds(p, p + 5) and B2 = bounds(p, p) ==> True
  // B1 = bounds(p, p + 5) and B2 = bounds(p + 5, p + 5) ==> True
  // B1 = bounds(p, p + 5) and B2 = bounds(p, p + 6) ==> False
  // B1 = bounds(p, p + 5) and B2 = bounds(p - 1, p + 1) ==> False
  // B1 = bounds(p, p + 5) and B2 = bounds(p + 6, p + 10) ==> False
  // B1 = bounds(p + 5, p + 6) and B2 = bounds(p, p + 5) ==> False
  // B1 = bounds(p, p + 5) and B2 = bounds(p, p + x) ==> False
  // B1 = bounds(p, p + x) and B2 = bounds(p, p + x + y) ==> False

  // To determine if B2 is a subrange of B1 we check if:
  // B2.Lower - B1.Lower >= 0 and B1.Upper - B2.Upper >= 0

  Expr *Lower1 = B1->getLowerExpr();
  Expr *Upper1 = B1->getUpperExpr();

  Expr *Lower2 = B2->getLowerExpr();
  Expr *Upper2 = B2->getUpperExpr();

  llvm::APSInt Zero(Ctx.getTargetInfo().getIntWidth(), 0);
  llvm::APSInt Offset;

  // Lex.GetExprIntDiff returns false if the two input expressions are not
  // comparable. This may happen if either of the expressions contains a
  // variable that is not present in the other.
  if (!Lex.GetExprIntDiff(Lower2, Lower1, Offset))
    return false;

  if (llvm::APSInt::compareValues(Offset, Zero) < 0)
    return false;

  if (!Lex.GetExprIntDiff(Upper1, Upper2, Offset))
    return false;

  return llvm::APSInt::compareValues(Offset, Zero) >= 0;
}

bool BoundsWideningUtil::ExistsNullCaseLabel(const CFGBlock *CurrBlock) const {
  for (const CFGBlock *SuccBlock : CurrBlock->succs()) {
    if (!SkipBlock(SuccBlock) && CaseLabelTestsForNull(SuccBlock))
      return true;
  }
  return false;
}

bool BoundsWideningUtil::IsSwitchCaseBlock(const CFGBlock *CurrBlock) const {
  const Stmt *BlockLabel = CurrBlock->getLabel();
  return BlockLabel &&
        (isa<CaseStmt>(BlockLabel) ||
         isa<DefaultStmt>(BlockLabel));
}

bool BoundsWideningUtil::CaseLabelTestsForNull(
  const CFGBlock *CurrBlock) const {

  const Stmt *BlockLabel = CurrBlock->getLabel();
  assert(BlockLabel && "invalid switch case");

  if (isa<DefaultStmt>(BlockLabel))
    return false;

  const auto *CS = dyn_cast<CaseStmt>(BlockLabel);

  // We mimic how clang (in SemaStmt.cpp) gets the value of a switch case. It
  // invokes EvaluateKnownConstInt and we do the same here. SemaStmt has
  // already extended/truncated the case value to fit the integer range and
  // EvaluateKnownConstInt gives us that value.
  const Expr *LHS = CS->getLHS();
  if (!LHS)
    return true;

  llvm::APSInt LHSVal = LHS->EvaluateKnownConstInt(Ctx);
  llvm::APSInt LHSZero (LHSVal.getBitWidth(), LHSVal.isUnsigned());
  if (llvm::APSInt::compareValues(LHSVal, LHSZero) == 0)
    return true;

  // If the case statement is not of the form "case LHS ... RHS" (a GNU
  // extension) then we are done. We only needed to check the LHS value which
  // is the case label for the current case. We return false because we have
  // determined that the current case label is non-null.
  if (!CS->caseStmtIsGNURange())
    return false;

  // If we reach are here it means we are in a range-base case statement of the
  // form "case LHS ... RHS" (a GNU extension).
  const Expr *RHS = CS->getRHS();
  if (!RHS)
    return true;

  llvm::APSInt RHSVal = RHS->EvaluateKnownConstInt(Ctx);
  llvm::APSInt RHSZero (RHSVal.getBitWidth(), RHSVal.isUnsigned());
  if (llvm::APSInt::compareValues(RHSVal, RHSZero) == 0)
    return true;

  // Return true if 0 is contained within the range [LHS, RHS].
  return (LHSVal <= LHSZero && RHSZero <= RHSVal) ||
         (LHSVal >= LHSZero && RHSZero >= RHSVal);
}

bool BoundsWideningUtil::IsFallthroughEdge(const CFGBlock *PredBlock,
                                           const CFGBlock *CurrBlock) const {
  // A fallthrough edge between two blocks is always a true edge. If PredBlock
  // has only one successor and CurrBlock is that successor then it means the
  // edge between PredBlock and CurrBlock is a fallthrough edge.
  return PredBlock->succ_size() == 1 &&
         CurrBlock == *(PredBlock->succs().begin());
}

bool BoundsWideningUtil::IsTrueEdge(const CFGBlock *PredBlock,
                                    const CFGBlock *CurrBlock) const {
  // Return false if PredBlock does not have any successors.
  if (PredBlock->succ_empty())
    return false;

  // A fallthrough edge is always a true edge.
  if (IsFallthroughEdge(PredBlock, CurrBlock))
    return true;

  // Get the last successor in the list of successors of PredBlock.
  const CFGBlock *LastSucc = *(PredBlock->succs().end() - 1);

  // If PredBlock has multiple successors, then a successor on a false edge
  // would always be last in the list of successors of PredBlock.
  if (CurrBlock == LastSucc)
    return PredBlock->succ_size() == 1;

  // Check if CurrBlock is a successor of PredBlock. If CurrBlock is not a
  // successor of PredBlock then there is no edge between PredBlock and
  // CurrBlock. So return false.
  for (const CFGBlock *SuccBlock : PredBlock->succs()) {
    if (CurrBlock == SuccBlock)
      return true;
  }

  return false;
}

void BoundsWideningUtil::GetModifiedVars(const Stmt *CurrStmt,
                                         VarSetTy &ModifiedVars) const {
  // Get all variables modified by CurrStmt or statements nested in CurrStmt.
  if (!CurrStmt)
    return;

  Expr *E = nullptr;

  // If the variable is modified using a unary operator, like ++I or I++.
  if (const auto *UO = dyn_cast<const UnaryOperator>(CurrStmt)) {
    if (UO->isIncrementDecrementOp()) {
      assert(UO->getSubExpr() && "invalid UnaryOperator expression");
      E = IgnoreCasts(UO->getSubExpr());
    }

  // Else if the variable is being assigned to, like I = ...
  } else if (const auto *BO = dyn_cast<const BinaryOperator>(CurrStmt)) {
    if (BO->isAssignmentOp())
      E = IgnoreCasts(BO->getLHS());
  }

  if (const auto *D = dyn_cast_or_null<DeclRefExpr>(E))
    if (const auto *V = dyn_cast_or_null<VarDecl>(D->getDecl()))
      ModifiedVars.insert(V);

  for (const Stmt *NestedStmt : CurrStmt->children())
    GetModifiedVars(NestedStmt, ModifiedVars);
}

void BoundsWideningUtil::GetNtPtrsWithVarsInLowerBounds(
  VarSetTy &Vars, VarSetTy &NtPtrsWithVarsInLowerBounds) const {

  // Get the set of variables that are pointers to null-terminated arrays and
  // in whose lower bounds expressions the variables in Vars occur.

  for (const VarDecl *V : Vars) {
    auto VarPtrIt = BoundsVarsLower.find(V);
    if (VarPtrIt != BoundsVarsLower.end()) {
      for (const VarDecl *Ptr : VarPtrIt->second)
        NtPtrsWithVarsInLowerBounds.insert(Ptr);
    }
  }
}

void BoundsWideningUtil::GetNtPtrsWithVarsInUpperBounds(
  VarSetTy &Vars, VarSetTy &NtPtrsWithVarsInUpperBounds) const {

  // Get the set of variables that are pointers to null-terminated arrays and
  // in whose upper bounds expressions the variables in Vars occur.

  for (const VarDecl *V : Vars) {
    auto VarPtrIt = BoundsVarsUpper.find(V);
    if (VarPtrIt != BoundsVarsUpper.end()) {
      for (const VarDecl *Ptr : VarPtrIt->second)
        if (!Ptr->isInvalidDecl() && IsNtArrayType(Ptr))
          NtPtrsWithVarsInUpperBounds.insert(Ptr);
    }
  }
}

Expr *BoundsWideningUtil::AddOffsetToExpr(Expr *E, unsigned Offset) const {
  // Returns the expression E + Offset.
  // Note: This function only returns the expression E + Offset and does not
  // actually evaluate the expression. So if E does not overflow then E +
  // Offset does not overflow here. However, E + Offset may later overflow when
  // the preorder AST performs constant folding, for example in case E is e +
  // MAX_INT and Offset is 1.

  const llvm::APInt
    APIntOff(Ctx.getTargetInfo().getPointerWidth(0), Offset);

  IntegerLiteral *WidenedOffset =
    ExprCreatorUtil::CreateIntegerLiteral(Ctx, APIntOff);

  return ExprCreatorUtil::CreateBinaryOperator(SemaRef, E, WidenedOffset,
                                               BinaryOperatorKind::BO_Add);
}

Expr *BoundsWideningUtil::GetDerefExpr(const Expr *TermCond) const {
  if (!TermCond)
    return nullptr;

  Expr *E = const_cast<Expr *>(TermCond);

  // According to C11 standard section 6.5.13, the logical AND Operator shall
  // yield 1 if both of its operands compare unequal to 0; otherwise, it yields
  // 0. The result has type int. An IntegralCast is generated for "if (e1 &&
  // e2)" Here we strip off the IntegralCast.
  if (auto *CE = dyn_cast<CastExpr>(E)) {
    if (CE->getCastKind() == CastKind::CK_IntegralCast)
      E = CE->getSubExpr();
  }

  if (auto *CE = dyn_cast<CastExpr>(E))
    if (CE->getCastKind() == CastKind::CK_LValueToRValue)
      E = CE->getSubExpr();

  E = IgnoreCasts(E);

  // A dereference expression can contain an array subscript or a pointer
  // dereference.

  // If a dereference expression is of the form "*(p + i)".
  if (auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() == UO_Deref)
      return IgnoreCasts(UO->getSubExpr());

  // Else if a dereference expression is an array access. An array access can
  // be written A[i] or i[A] (both are equivalent).  getBase() and getIdx()
  // always present the normalized view: A[i]. In this case getBase() returns
  // "A" and getIdx() returns "i".
  } else if (auto *AE = dyn_cast<ArraySubscriptExpr>(E)) {
    return ExprCreatorUtil::CreateBinaryOperator(SemaRef, AE->getBase(),
                                                 AE->getIdx(),
                                                 BinaryOperatorKind::BO_Add);
  }
  return nullptr;
}

void BoundsWideningUtil::GetNtPtrsInExpr(Expr *E,
                                         VarSetTy &NtPtrsInExpr) const {
  // Get all variables in the expression that are pointers to null-terminated
  // arrays.

  // TODO: Currently this function returns a subset of all variables that occur
  // in an expression and that are pointers to null-terminated arrays. It
  // returns only the top-level variables that occur in an expression. For
  // example, in the expression "a + b - c" it returns {a, b, c} if a, b, c are
  // all pointers to null-terminated arrays. It does not return variables that
  // are members of a struct or arguments of a function call, etc.  In future,
  // when we change this analysis to use AbstractSets, this function can handle
  // an expression like "s->f + func(x)" and return {s->f, x} if s->f and x are
  // pointers to null-terminated arrays.

  if (!E)
    return;

  if (auto *CE = dyn_cast<CastExpr>(E))
    if (CE->getCastKind() == CastKind::CK_LValueToRValue)
      E = CE->getSubExpr();

  E = IgnoreCasts(E);

  // Get variables in an expression like *e.
  if (const auto *UO = dyn_cast<const UnaryOperator>(E)) {
    GetNtPtrsInExpr(UO->getSubExpr(), NtPtrsInExpr);

  // Get variables in an expression like e1 + e2.
  } else if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    GetNtPtrsInExpr(BO->getLHS(), NtPtrsInExpr);
    GetNtPtrsInExpr(BO->getRHS(), NtPtrsInExpr);
  }

  // If the variable is a pointer to a null-terminated array add the variable
  // to NtPtrsInExpr.
  if (const auto *D = dyn_cast<DeclRefExpr>(E)) {
    if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
      if (!V->isInvalidDecl() && IsNtArrayType(V))
        NtPtrsInExpr.insert(V);
  }
}

Expr *BoundsWideningUtil::IgnoreCasts(const Expr *E) const {
  return Lex.IgnoreValuePreservingOperations(Ctx, const_cast<Expr *>(E));
}

bool BoundsWideningUtil::SkipBlock(const CFGBlock *B) const {
  return !B || B == &Cfg->getExit();
}

bool BoundsWideningUtil::IsNtArrayType(const VarDecl *V) const {
  return V && (V->getType()->isCheckedPointerNtArrayType() ||
               V->getType()->isNtCheckedArrayType());
}

// Common templated set operation functions.
template<class T, class U>
T BoundsWideningUtil::Difference(T &A, U &B) const {
  if (!A.size() || !B.size())
    return A;

  auto CopyA = A;
  for (auto Item : A) {
    if (B.count(Item))
      CopyA.erase(Item);
  }
  return CopyA;
}

template<class T>
T BoundsWideningUtil::Union(T &A, T &B) const {
  auto CopyA = A;
  for (auto Item : B)
    CopyA.insert(Item);

  return CopyA;
}

template<class T>
T BoundsWideningUtil::Intersect(T &A, T &B) const {
  if (!A.size() || !B.size())
    return T();

  auto CopyA = A;
  for (auto Item : A) {
    if (!B.count(Item))
      CopyA.erase(Item);
  }
  return CopyA;
}

template<class T>
bool BoundsWideningUtil::IsEqual(T &A, T &B) const {
  return A.size() == B.size() &&
         A.size() == Intersect(A, B).size();
}

// Template specializations of common set operation functions.
template<>
BoundsMapTy BoundsWideningUtil::Difference<BoundsMapTy, VarSetTy>(
  BoundsMapTy &A, VarSetTy &B) const {

  if (!A.size() || !B.size())
    return A;

  auto CopyA = A;
  for (auto VarBoundsPair : A) {
    if (B.count(VarBoundsPair.first))
      CopyA.erase(VarBoundsPair.first);
  }
  return CopyA;
}

template<>
BoundsMapTy BoundsWideningUtil::Intersect<BoundsMapTy>(
  BoundsMapTy &A, BoundsMapTy &B) const {

  if (!A.size() || !B.size())
    return BoundsMapTy();

  auto CopyA = A;
  for (auto VarBoundsPair : B) {
    const VarDecl *V = VarBoundsPair.first;
    auto VarBoundsIt = CopyA.find(V);
    if (VarBoundsIt == CopyA.end()) {
      CopyA.erase(V);
      continue;
    }

    RangeBoundsExpr *BoundsA = VarBoundsIt->second;
    RangeBoundsExpr *BoundsB = VarBoundsPair.second;

    // Currently, CopyA[V] is BoundsA. Set CopyA[V] to BoundsB if:
    // 1. BoundsA is Top, or
    // 2. BoundsB is not Top and BoundsB is a subrange of BoundsA.
    if (BoundsA == BoundsWideningAnalysis::Top)
      CopyA[V] = BoundsB;
    else if (BoundsB != BoundsWideningAnalysis::Top &&
             IsSubRange(BoundsA, BoundsB))
      CopyA[V] = BoundsB;
  }
  return CopyA;
}

template<>
BoundsMapTy BoundsWideningUtil::Union<BoundsMapTy>(
  BoundsMapTy &A, BoundsMapTy &B) const {

  auto CopyA = A;
  for (auto VarBoundsPair : B)
    CopyA[VarBoundsPair.first] = VarBoundsPair.second;

  return CopyA;
}

template<>
bool BoundsWideningUtil::IsEqual<BoundsMapTy>(
  BoundsMapTy &A, BoundsMapTy &B) const {

  if (A.size() != B.size())
    return false;

  auto CopyA = A;
  for (auto VarBoundsPair : B) {
    const VarDecl *V = VarBoundsPair.first;

    auto VarBoundsIt = CopyA.find(V);
    if (VarBoundsIt == CopyA.end())
      return false;

    RangeBoundsExpr *BoundsA = VarBoundsIt->second;
    RangeBoundsExpr *BoundsB = VarBoundsPair.second;

    if (BoundsA == BoundsWideningAnalysis::Top ||
        BoundsB == BoundsWideningAnalysis::Top)
      return BoundsA == BoundsB;

    if (!Lex.CompareExprSemantically(BoundsA->getUpperExpr(),
                                     BoundsB->getUpperExpr()))
      return false;
  }
  return true;
}
// end of methods for the BoundsWideningUtil class.

} // end namespace clang
