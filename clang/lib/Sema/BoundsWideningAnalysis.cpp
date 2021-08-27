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

void BoundsWideningAnalysis::WidenBounds(FunctionDecl *FD,
                                         StmtSetTy NestedStmts) {
  assert(Cfg && "expected CFG to exist");

  // Initialize the list of variables that are pointers to null-terminated
  // arrays. This list will be initialized with the variables that are passed
  // as parameters to the function.
  InitNullTermPtrsInFunc(FD);

  // Note: By default, PostOrderCFGView iterates in reverse order. So we always
  // get a reverse post order when we iterate PostOrderCFGView.
  for (const CFGBlock *B : PostOrderCFGView(Cfg)) {
    // SkipBlock will skip all null blocks and the exit block. PostOrderCFGView
    // does not traverse any unreachable blocks. So at the end of this loop
    // BlockMap only contains reachable blocks.
    if (BWUtil.SkipBlock(B))
      continue;

    // Create a mapping from CFGBlock to ElevatedCFGBlock.
    auto EB = new ElevatedCFGBlock(B);
    BlockMap[B] = EB;

    // Compute Gen and Kill sets for the block and statements in the block.
    ComputeGenKillSets(EB, NestedStmts);

    // Initialize the In and Out sets for the block.
    InitBlockInOutSets(FD, EB);
  }

  // WorkList store the blocks that remain to be processed for the fixedpoint
  // computation. WorkList is a queue. So we maintain the reverse post order
  // when we iterate WorkList.
  // We initialize WorkList with the successor blocks of the entry block.
  WorkListTy WorkList;
  AddSuccsToWorkList(&Cfg->getEntry(), WorkList);

  // Compute the In and Out sets for blocks. This is the fixedpoint computation
  // for the dataflow analysis.
  while (!WorkList.empty()) {
    ElevatedCFGBlock *EB = WorkList.next();
    WorkList.remove(EB);

    bool Changed = false;
    Changed |= ComputeInSet(EB);
    Changed |= ComputeOutSet(EB);

    // If the In or the Out set of the block has changed then we add all
    // successors of the block to the WorkList.
    if (Changed)
      AddSuccsToWorkList(EB->Block, WorkList);
  }
}

BoundsMapTy BoundsWideningAnalysis::GetOutOfLastStmt(
  ElevatedCFGBlock *EB) const {

  // Traverse statements in the block and compute the SmtOut set for each
  // statement and return the StmtOut for the last statement of the block.

  BoundsMapTy StmtOut = EB->In;

  for (CFGBlock::const_iterator I = EB->Block->begin(),
                                E = EB->Block->end();
       I != E; ++I) {
    CFGElement Elem = *I;
    if (Elem.getKind() != CFGElement::Statement)
      continue;
    const Stmt *CurrStmt = Elem.castAs<CFGStmt>().getStmt();
    if (!CurrStmt)
      continue;

    // The In of the current statement is the value of StmtOut computed so far.
    BoundsMapTy InOfCurrStmt = StmtOut;

    // If this is the last statement of the current block, then at this point
    // InOfCurrStmt contains the Out set of the second last statement of the
    // block.  This is equal to the In set for the last statement of this
    // block. So we set InOfLastStmt to InOfCurrStmt.
    if (CurrStmt == EB->LastStmt)
      EB->InOfLastStmt = InOfCurrStmt;

    // StmtOut = (InOfCurrStmt - StmtKill) u StmtGen.
    auto Diff = BWUtil.Difference(InOfCurrStmt, EB->StmtKill[CurrStmt]);
    StmtOut = BWUtil.Union(Diff, EB->StmtGen[CurrStmt]);

    // Update StmtOut based on the invertibility of CurrStmt.
    auto InvStmtIt = EB->InvertibleStmts.find(CurrStmt);
    if (InvStmtIt == EB->InvertibleStmts.end())
      continue;

    // At CurrStmt, we need to replace the ModifiedLValue with the
    // OriginalLValue in the bounds of every null-terminated array occurring in
    // PtrsWithAffectedBounds.
    auto ValuesToReplaceInBounds = InvStmtIt->second;

    Expr *ModifiedLValue = std::get<0>(ValuesToReplaceInBounds);
    Expr *OriginalLValue = std::get<1>(ValuesToReplaceInBounds);
    VarSetTy PtrsWithAffectedBounds = std::get<2>(ValuesToReplaceInBounds);

    CheckedScopeSpecifier CSS = CurrStmt->getCheckedScopeSpecifier();

    for (const VarDecl *V : PtrsWithAffectedBounds) {
      auto StmtInIt = InOfCurrStmt.find(V);
      if (StmtInIt == InOfCurrStmt.end())
        continue;

      BoundsExpr *SrcBounds = StmtInIt->second;

      // Replace the modified LValue with the original LValue in the bounds
      // expression of V.
      BoundsExpr *AdjustedBounds =
        BoundsUtil::ReplaceLValueInBounds(SemaRef, SrcBounds, ModifiedLValue,
                                          OriginalLValue, CSS);
      RangeBoundsExpr *AdjustedRangeBounds =
        dyn_cast_or_null<RangeBoundsExpr>(AdjustedBounds);

      if (!AdjustedRangeBounds)
        llvm_unreachable("Invalid RangeBoundsExpr!");

      // In the bounds widening analysis the widest value of an upper bounds
      // expression is Top, whereas the narrowest value is the declared upper
      // bound. This means that the upper bound can/should never become
      // narrower than the declared upper bound.
      // So in case we have an invertible statement that modifies a variable
      // occurring in the bounds expression of a null-terminated array we
      // should always reset the bounds to the declared upper bound except when
      // replacement of the modified LValue with its original LValue results in
      // a bounds expression which is strictly wider than the declared upper
      // bound.
      // So we will proceed only if AdjustedRangeBounds is wider than
      // StmtOut[V] which contains the declared bounds of V at this point. For
      // example:

      // Let DB = Declared bounds, AB = Adjusted bounds.

      // DB = (p, p + len), AB = (p, p + len + 1)
      //   ==> AB is wider than DB ==> set bounds of V to AB.

      // DB = (p, p + len), AB = (p, p + len - 1)
      //   ==> AB is not wider than DB ==> set bounds of V to DB.

      // DB = (p, p + len), AB = (p, p + len + i)
      //   ==> AB cannot be compared to DB ==> set bounds of V to DB.

      if (!BWUtil.IsSubRange(AdjustedRangeBounds, StmtOut[V]))
        continue;

      // Update the bounds of V with the adjusted bounds.
      StmtOut[V] = AdjustedRangeBounds;

      // Store the adjusted bounds for the current statement. We will use these
      // when clients invoke GetStmtIn or GetStmtOut.
      EB->AdjustedBounds[CurrStmt][V] = AdjustedRangeBounds;
    }
  }
  return StmtOut;
}

void BoundsWideningAnalysis::ComputeGenKillSets(ElevatedCFGBlock *EB,
                                                StmtSetTy NestedStmts) {
  const Stmt *PrevStmt = nullptr;

  // Traverse statements in the block and compute Gen and Kill sets for each
  // statement.
  for (CFGBlock::const_iterator I = EB->Block->begin(),
                                E = EB->Block->end();
       I != E; ++I) {
    CFGElement Elem = *I;
    if (Elem.getKind() != CFGElement::Statement)
      continue;
    const Stmt *CurrStmt = Elem.castAs<CFGStmt>().getStmt();
    if (!CurrStmt)
      continue;

    // Compute Gen and Kill sets for the current statement.
    ComputeStmtGenKillSets(EB, CurrStmt, NestedStmts);

    // Update the list of null-terminated arrays in the function with the
    // null-terminated arrays that became part of Gen set for the current
    // statement.
    UpdateNullTermPtrsInFunc(EB, CurrStmt);

    EB->PrevStmtMap[CurrStmt] = PrevStmt;
    PrevStmt = CurrStmt;

    // We need the last statement of the block during In/Out set computations.
    // Let the current statement be the last statement of the block. At the end
    // of this loop EB->LastStmt will contain the actual last statement of the
    // block (if the block contains at least one statement, else nullptr).
    EB->LastStmt = CurrStmt;
  }
}

void BoundsWideningAnalysis::ComputeStmtGenKillSets(ElevatedCFGBlock *EB,
                                                    const Stmt *CurrStmt,
                                                    StmtSetTy NestedStmts) {
  // Initialize the Gen and Kill sets for the statement.
  EB->StmtGen[CurrStmt] = BoundsMapTy();
  EB->StmtKill[CurrStmt] = VarSetTy();

  BoundsMapTy VarsAndBounds;

  const CFGBlock *CurrBlock = EB->Block;
  const Stmt *TermStmt = CurrBlock->getTerminatorStmt();

  // Determine whether CurrStmt generates a dataflow fact.

  // A conditional statement that dereferences a variable that is a pointer to
  // a null-terminated array can generate a dataflow fact. For example: if (*(p
  // + 1)) The conditional will always be the terminator statement of the block.
  if (TermStmt && !isa<AsmStmt>(TermStmt) &&
      CurrStmt == CurrBlock->getLastCondition()) {
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

  // A modification of a variable that occurs either in the lower bounds
  // expression or an upper bounds expression of a null-terminated array does
  // the following:
  // 1. Kills the widened bounds of that null-terminated array, and
  // 2. Resets the bounds of the null-terminated array to its declared bounds.
  // For example:
  // int i;
  // _Nt_array_ptr<char> p : bounds(p, p + i);
  // if (*(p + i)) { // widen bounds of p to bounds(p, p + i + 1)

  //   i = 0; // kill the widened bounds of p, and
  //          // reset bounds of p to bounds(p, p + i)
  // }

  // If a variable modified by CurrStmt occurs in the bounds expression of a
  // null-terminated array then the bounds of that null-terminated array should
  // be killed and its bounds should be reset to its declared bounds.
  // Note: Skip top-level statements that are nested in another top-level
  // statement.
  if (NestedStmts.find(CurrStmt) == NestedStmts.end())
    GetVarsAndBoundsForModifiedVars(EB, CurrStmt, VarsAndBounds);

  // Using the mapping of variables to bounds expressions in VarsAndBounds fill
  // the Gen and Kill sets for the current statement.
  FillStmtGenKillSets(EB, CurrStmt, VarsAndBounds);
}

bool BoundsWideningAnalysis::ComputeInSet(ElevatedCFGBlock *EB) {
  const CFGBlock *CurrBlock = EB->Block;
  auto OrigIn = EB->In;

  // Iterate through all the predecessor blocks of EB.
  for (const CFGBlock *PredBlock : CurrBlock->preds()) {
    auto BlockIt = BlockMap.find(PredBlock);
    if (BlockIt == BlockMap.end())
      continue;

    ElevatedCFGBlock *PredEB = BlockIt->second;

    // To compute the In set for the block we need to intersect the Out sets of
    // all preds of the current block. In order to simplify the intersection
    // operation we "prune" (or pre-process) the Out sets of preds here
    // according to various conditions. The intersection then happens on the
    // pruned Out sets.
    BoundsMapTy PrunedOutSet = PruneOutSet(PredEB, EB);

    EB->In = BWUtil.Intersect(EB->In, PrunedOutSet);
  }

  // Return true if the In set has changed, false otherwise.
  return !BWUtil.IsEqual(OrigIn, EB->In);
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

  // Does the terminating condition of the pred block test for a null value.
  bool DoesTermCondCheckNull = PredEB->TermCondInfo.IsCheckNull;

  // Get the In of the last statement in the pred block. If the pred
  // block does not have any statements then InOfLastStmtOfPred is set to
  // the In set of the pred block.
  BoundsMapTy InOfLastStmtOfPred = PredEB->InOfLastStmt;

  // Check if the current block is a case of a switch-case.
  bool IsSwitchCase = BWUtil.IsSwitchCaseBlock(CurrBlock, PredBlock);

  // If the current block is a case of a switch-case, check if the case label
  // tests for null. CaseLabelTestsForNull returns false for the default case.
  bool IsCaseLabelNull = IsSwitchCase &&
                         BWUtil.CaseLabelTestsForNull(CurrBlock);

  // PredEB->Out may contain the widened bounds "E + 1" for some variable V.
  // Here, we need to determine if "E + 1" should make it to the In set of
  // the current block. To determine this, we need to check if the
  // dereference is at the upper bound and if we are on a true edge. Else we
  // will reset the bounds of V in PrunedOutSet to the bounds of V in
  // InOfLastStmtOfPred.
  for (auto VarBoundsPair : PredEB->Out) {
    const VarDecl *V = VarBoundsPair.first;
    auto StmtInIt = InOfLastStmtOfPred.find(V);

    // If V is not present in In of the last statement (maybe as a result of
    // being killed before the last statement) then V cannot be widened in this
    // block. So we remove V from the computation of the In set for this block.
    // For example:

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

    if (StmtInIt == InOfLastStmtOfPred.end()) {
      PrunedOutSet.erase(V);
      continue;
    }

    RangeBoundsExpr *BoundsOfVInStmtIn = StmtInIt->second;

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

    if (!IsSwitchCase) {
      // if (*p != 0) {
      //   IsEdgeTrue = True, DoesTermCondCheckNull = False
      //     ==> widen bounds of p
      //
      // } else {
      //   IsEdgeTrue = False, DoesTermCondCheckNull = False
      //     ==> do not widen bounds of p
      // }

      // if (*p == 0) {
      //   IsEdgeTrue = True, DoesTermCondCheckNull = True
      //     ==> do not widen bounds of p
      //
      // } else {
      //   IsEdgeTrue = False, DoesTermCondCheckNull = True
      //     ==> widen bounds of p
      // }
      if (IsEdgeTrue == DoesTermCondCheckNull) {
        PrunedOutSet[V] = BoundsOfVInStmtIn;
        continue;
      }
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
      PredEB->TermCondInfo.DerefExpr && BoundsOfVInStmtIn != Top &&
      Lex.CompareExprSemantically(PredEB->TermCondInfo.DerefExpr,
                                  BoundsOfVInStmtIn->getUpperExpr());

    if (!IsDerefAtUpperBound) {
      PrunedOutSet[V] = BoundsOfVInStmtIn;
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
        PrunedOutSet[V] = BoundsOfVInStmtIn;

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
        PrunedOutSet[V] = BoundsOfVInStmtIn;
    }
  }

  return PrunedOutSet;
}

bool BoundsWideningAnalysis::ComputeOutSet(ElevatedCFGBlock *EB) {
  auto OrigOut = EB->Out;

  // Set the Out set of the block to the Out set for the last statement of the
  // current block. If the current block does not have any statements
  // GetOutOfLastStmt returns the In set of the block.
  EB->Out = GetOutOfLastStmt(EB);

  // Return true if the Out set has changed, false otherwise.
  return !BWUtil.IsEqual(OrigOut, EB->Out);
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
    for (const VarDecl *V : AllNullTermPtrsInFunc) {
      EB->In[V] = Top;
      EB->Out[V] = Top;
      EB->InOfLastStmt[V] = Top;
    }
  }
}

void BoundsWideningAnalysis::AddSuccsToWorkList(const CFGBlock *CurrBlock,
                                                WorkListTy &WorkList) {
  if (!CurrBlock)
    return;

  for (const CFGBlock *SuccBlock : CurrBlock->succs()) {
    if (!BWUtil.SkipBlock(SuccBlock))
      WorkList.append(BlockMap[SuccBlock]);
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

  // CurrStmt will be null if:
  // 1. This method is called with a null value for CurrStmt, or
  // 2. GetStmtIn calls this method to get the In set for the first statement
  // of the block. Because the Out of the previous statement is equal to the In
  // of the current statement, GetStmtIn will call this function with the
  // previous statement of the first statment (which would be null).

  // In both cases we will set the OutOfPrevStmt to the In set of the block and
  // return it.
  if (!CurrStmt) {
    EB->OutOfPrevStmt = EB->In;
    return EB->In;
  }

  // If we are here it means the client wants the Out set for the first
  // statement of the block (that is the reason PrevStmtMap[CurrStmt] is null).
  // In this case, we set OutOfPrevStmt to the In set of the block and then
  // apply the regular (In - Kill) u Gen computation on it.
  if (!EB->PrevStmtMap[CurrStmt])
    EB->OutOfPrevStmt = EB->In;

  auto Diff = BWUtil.Difference(EB->OutOfPrevStmt, EB->StmtKill[CurrStmt]);
  auto StmtOut = BWUtil.Union(Diff, EB->StmtGen[CurrStmt]);

  // Account for bounds which are killed by the current statement but which may
  // have been adjusted using invertibility of the statement. This function
  // modifies StmtOut.
  UpdateAdjustedBounds(EB, CurrStmt, StmtOut);

  EB->OutOfPrevStmt = StmtOut;
  return StmtOut;
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

BoundsMapTy BoundsWideningAnalysis::GetBoundsWidenedAndNotKilled(
  const CFGBlock *B, const Stmt *CurrStmt) const {

  auto BlockIt = BlockMap.find(B);
  if (BlockIt == BlockMap.end())
    return BoundsMapTy();

  ElevatedCFGBlock *EB = BlockIt->second;

  BoundsMapTy InOfCurrStmt = GetStmtIn(B, CurrStmt);
  auto BoundsWidenedAndNotKilled = BWUtil.Difference(InOfCurrStmt,
                                                     EB->StmtKill[CurrStmt]);

  // Account for bounds which are killed by the current statement but which may
  // have been adjusted using invertibility of the statement. This function
  // modifies BoundsWidenedAndNotKilled.
  UpdateAdjustedBounds(EB, CurrStmt, BoundsWidenedAndNotKilled);
  return BoundsWidenedAndNotKilled;
}

void BoundsWideningAnalysis::InitNullTermPtrsInFunc(FunctionDecl *FD) {
  // Initialize the list of variables that are pointers to null-terminated
  // arrays to the null-terminated arrays that are passed as parameters to the
  // function.
  for (const ParmVarDecl *PD : FD->parameters()) {
    if (BWUtil.IsNtArrayType(PD))
      AllNullTermPtrsInFunc.insert(PD);
  }
}

void BoundsWideningAnalysis::UpdateNullTermPtrsInFunc(ElevatedCFGBlock *EB,
                                                      const Stmt *CurrStmt) {
  // Add variables occurring in StmtGen for the current statement to the list
  // of variables that are pointers to null-terminated arrays.
  for (auto VarBoundsPair : EB->StmtGen[CurrStmt])
    AllNullTermPtrsInFunc.insert(VarBoundsPair.first);
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
    VarsAndBounds[V] = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds);
  }
}

void BoundsWideningAnalysis::GetVarsAndBoundsInWhereClause(
  ElevatedCFGBlock *EB, WhereClause *WC, BoundsMapTy &VarsAndBounds) {

  if (!WC)
    return;

  for (const auto *Fact : WC->getFacts()) {
    auto *F = dyn_cast<BoundsDeclFact>(Fact);
    if (!F)
      continue;

    VarDecl *V = F->getVarDecl();
    if (BWUtil.IsNtArrayType(V)) {
      BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(F);
      VarsAndBounds[V] = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds);
    }
  }
}

void BoundsWideningAnalysis::GetVarsAndBoundsInPtrDeref(
  ElevatedCFGBlock *EB, BoundsMapTy &VarsAndBounds) {

  // Get the terminating condition of the block.
  // The CFG for a compound condition like "if (e1 && e2 && e3)" is as follows:

  // B1->B2 (edge condition e1)
  // B2->B3 (edge condition e2)
  // B3->B4 (edge condition e3)

  // So we see that each condition becomes an edge in the CFG.
  // getTerminatorStmt() would give us the entire condition "e1 && e2 && e3"
  // while getLastCondition() would give us e1 for the first edge e2 for the
  // second and so on.
  const Expr *TermCond = EB->Block->getLastCondition();
  if (!TermCond)
    return;

  // Get information about the terminating condition such as the dereference
  // expression and whether the condition tests for a null value.
  EB->TermCondInfo = BWUtil.GetTermCondInfo(TermCond);

  // If the terminating condition does not contain a dereference (or an array
  // subscript) we cannot possibly widen a pointer.
  if (!EB->TermCondInfo.DerefExpr)
    return;

  // Get the variable in the expression that is a pointer to a null-terminated
  // array. For example: On a dereference expression like "*(p + i + j + 1)"
  // GetNullTermPtrInExpr() will return p if p is a pointer to a null-terminated
  // array.
  // Note: We assume that a dereference expression can only contain at most one
  // pointer to a null-terminated array.
  const VarDecl *NullTermPtrInExpr =
    BWUtil.GetNullTermPtrInExpr(EB->TermCondInfo.DerefExpr);
  if (!NullTermPtrInExpr)
    return;

  // Get all variables that are pointers to null-terminated arrays and in whose
  // upper bounds expressions the variable NullTermPtrInExpr occurs. For
  // example:

  // _Nt_array_ptr<char> p : bounds(p, p);
  // _Nt_array_ptr<char> q : bounds(q, p + i);
  // _Nt_array_ptr<char> r : bounds(r, p + 2);
  // _Nt_array_ptr<char> s : bounds(p, s);

  // On a dereference expression like "*(p + i + j + 1)"
  // GetPtrsWithVarsInUpperBounds() will return {p, q, r} because p
  // occurs in the upper bounds expressions of p, q and r.

  VarSetTy Vars;
  Vars.insert(NullTermPtrInExpr);

  VarSetTy PtrsWithAffectedBounds;
  BWUtil.GetPtrsWithVarsInUpperBounds(Vars, PtrsWithAffectedBounds);

  // Now, the bounds of all variables in PtrsWithAffectedBounds can potentially
  // be widened to bounds(lower, DerefExpr + 1).

  for (const VarDecl *V : PtrsWithAffectedBounds) {
    BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(V);
    RangeBoundsExpr *R = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds);

    // DerefExpr potentially widens V. So we need to add "V:bounds(lower,
    // DerefExpr + 1)" to the Gen set.

    // TODO: Here, we always consider the lower bound from the declared bounds
    // of V. But this may be incorrect in case a where clause redeclares the
    // lower bound of V. This needs to be handled.

    RangeBoundsExpr *WidenedBounds =
      new (Ctx) RangeBoundsExpr(R->getLowerExpr(),
        BWUtil.AddOffsetToExpr(EB->TermCondInfo.DerefExpr, 1),
        SourceLocation(), SourceLocation());

    VarsAndBounds[V] = WidenedBounds;
  }
}

void BoundsWideningAnalysis::GetVarsAndBoundsForModifiedVars(
  ElevatedCFGBlock *EB, const Stmt *CurrStmt, BoundsMapTy &VarsAndBounds) {

  // Get variables modified by CurrStmt.
  VarSetTy ModifiedVars;
  BWUtil.GetModifiedVars(CurrStmt, ModifiedVars);

  // Get the set of variables that are pointers to null-terminated arrays and
  // in whose lower and upper bounds expressions the modified variables occur.
  VarSetTy PtrsWithAffectedBounds;
  BWUtil.GetPtrsWithVarsInLowerBounds(ModifiedVars,
                                      PtrsWithAffectedBounds);
  BWUtil.GetPtrsWithVarsInUpperBounds(ModifiedVars,
                                      PtrsWithAffectedBounds);

  // For each null-terminated array we need to reset the bounds to its declared
  // bounds.
  for (const VarDecl *V : PtrsWithAffectedBounds) {
    BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(V);
    VarsAndBounds[V] = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds);
  }

  // If the modification of a variable by the current statement affects the
  // bounds of a null-terminated array, then check invertibility of the
  // statement. If the statement is invertible then store the statement, the
  // modified LValue, the original LValue and the set of null-terminated arrays
  // whose bounds are affected by the statement. We will use this info in the
  // computation of the Out sets of statements which will, in turn be used to
  // compute the Out sets of blocks.
  CheckStmtInvertibility(EB, CurrStmt, PtrsWithAffectedBounds);
}

void BoundsWideningAnalysis::CheckStmtInvertibility(ElevatedCFGBlock *EB,
  const Stmt *CurrStmt, VarSetTy PtrsWithAffectedBounds) const {

  // If the variables modified by the current statement do not affect the
  // bounds of any null-terminated array we do not need to check statement
  // invertibility.
  if (PtrsWithAffectedBounds.size() == 0)
    return;

  Expr *ModifiedLValue = nullptr;
  Expr *ModifyingExpr = nullptr;

  // If the current statement is a unary inc/dec. For example: ++len
  if (const auto *UO = dyn_cast<const UnaryOperator>(CurrStmt)) {
    if (!UO->isIncrementDecrementOp())
      return;

    // Get the LValue being incremented/decremented. For example: len
    ModifiedLValue = UO->getSubExpr();
    if (!ModifiedLValue)
      return;

    // Normalize the inc/dec of the LValue to LValue +/- 1.
    // For example: ++len is normalized to len + 1
    //              len-- is normalized to len - 1
    IntegerLiteral *One = ExprCreatorUtil::CreateIntegerLiteral(
                            Ctx, 1, ModifiedLValue->getType());

    BinaryOperatorKind OpKind = UnaryOperator::isIncrementOp(UO->getOpcode()) ?
                                BO_Add : BO_Sub;

    // Here ModifyingExpr will be of the form len +/- 1.
    ModifyingExpr =
      ExprCreatorUtil::CreateBinaryOperator(SemaRef, ModifiedLValue,
                                            One, OpKind);

    // Else if the current statement is an assignment statement. For example:
    // len = e1
  } else if (const auto *BO = dyn_cast<const BinaryOperator>(CurrStmt)) {
    if (!BO->isAssignmentOp())
      return;

    // ModifiedLValue is len.
    ModifiedLValue = BO->getLHS();
    // ModifyingExpr is e1.
    ModifyingExpr = BO->getRHS();

    BinaryOperatorKind OpKind = BO->getOpcode();
    // If the current statement is of the form len += e1.
    if (OpKind == BO_AddAssign || OpKind == BO_SubAssign) {
      OpKind = OpKind == BO_AddAssign ? BO_Add : BO_Sub;

      // Normalize the ModifyingExpr to len + e1.
      ModifyingExpr =
        ExprCreatorUtil::CreateBinaryOperator(SemaRef, ModifiedLValue,
                                              ModifyingExpr, OpKind);
    }
  }

  if (!ModifiedLValue || !ModifyingExpr)
    return;

  CastExpr *Target =
    ExprCreatorUtil::CreateImplicitCast(SemaRef, ModifiedLValue,
                                        CK_LValueToRValue,
                                        ModifiedLValue->getType());

  // Check if the modifying expr is invertible w.r.t. the modified LValue.
  if (InverseUtil::IsInvertible(SemaRef, ModifiedLValue, ModifyingExpr)) {
    // Get the original LValue for the modified LValue. For example, for len++
    // the original LValue would be len - 1.
    Expr *OriginalLValue = InverseUtil::Inverse(SemaRef, ModifiedLValue,
                                                Target, ModifyingExpr);

    // Store the modified LValue, the original LValue and the set of
    // null-terminated arrays whose bounds expressions are affected by the
    // LValue being modified.
    if (OriginalLValue)
      EB->InvertibleStmts[CurrStmt] = std::make_tuple(ModifiedLValue,
                                                      OriginalLValue,
                                                      PtrsWithAffectedBounds);
  }
}

void BoundsWideningAnalysis::UpdateAdjustedBounds(
  ElevatedCFGBlock *EB, const Stmt *CurrStmt, BoundsMapTy &StmtOut) const {

  auto AdjBoundsIt = EB->AdjustedBounds.find(CurrStmt);
  if (AdjBoundsIt == EB->AdjustedBounds.end())
    return;

  for (auto Item : AdjBoundsIt->second) {
    const VarDecl *V = Item.first;
    RangeBoundsExpr *AdjustedBounds = Item.second;

    StmtOut[V] = AdjustedBounds;
  }
}

void BoundsWideningAnalysis::PrintVarSet(VarSetTy VarSet,
                                         int PrintOption) const {
  if (VarSet.size() == 0) {
    if (PrintOption == 0)
      OS << "<no widening>\n";
    else
      OS << "    {}\n";
    return;
  }

  // A VarSetTy has const iterator. So we cannot simply sort a VarSetTy and
  // need to copy the elements to a vector to sort.
  std::vector<const VarDecl *> Vars(VarSet.begin(), VarSet.end());

  llvm::sort(Vars.begin(), Vars.end(),
    [](const VarDecl *A, const VarDecl *B) {
       return A->getQualifiedNameAsString().compare(
              B->getQualifiedNameAsString()) < 0;
    });

  for (const VarDecl *V : Vars)
    OS << "    " << V->getQualifiedNameAsString() << "\n";
}

void BoundsWideningAnalysis::PrintBoundsMap(BoundsMapTy BoundsMap,
                                            int PrintOption) const {
  if (BoundsMap.size() == 0) {
    if (PrintOption == 0)
      OS << "<no widening>\n";
    else
      OS << "    {}\n";
    return;
  }

  std::vector<const VarDecl *> Vars;
  for (auto VarBoundsPair : BoundsMap)
    Vars.push_back(VarBoundsPair.first);

  llvm::sort(Vars.begin(), Vars.end(),
    [](const VarDecl *A, const VarDecl *B) {
       return A->getQualifiedNameAsString().compare(
              B->getQualifiedNameAsString()) < 0;
    });

  for (const VarDecl *V : Vars) {
    OS << "    " << V->getQualifiedNameAsString() << ": ";

    RangeBoundsExpr *Bounds = BoundsMap[V];

    if (Bounds == Top) {
      OS << "Top\n";
      continue;
    }

    Expr *Lower = Bounds->getLowerExpr();
    Expr *Upper = Bounds->getUpperExpr();

    OS << "bounds(";
    Lower->printPretty(OS, nullptr, Ctx.getPrintingPolicy());
    OS << ", ";
    Upper->printPretty(OS, nullptr, Ctx.getPrintingPolicy());
    OS << ")\n";
  }
}

void BoundsWideningAnalysis::PrintStmt(const Stmt *CurrStmt) const {
  if (!CurrStmt) {
    OS << "\n";
    return;
  }

  std::string Str;
  llvm::raw_string_ostream SS(Str);
  CurrStmt->printPretty(SS, nullptr, Ctx.getPrintingPolicy());

  OS << SS.str();
  if (SS.str().back() != '\n')
    OS << "\n";
}

void BoundsWideningAnalysis::DumpWidenedBounds(FunctionDecl *FD,
                                               int PrintOption) {

  OS << "\n--------------------------------------";
  // Print the function name.
  OS << "\nFunction: " << FD->getName();

  for (const CFGBlock *CurrBlock : GetOrderedBlocks()) {
    unsigned BlockID = CurrBlock->getBlockID();

    // Print the current block number.
    OS << "\nBlock: B" << BlockID;
    if (CurrBlock == &Cfg->getEntry())
      OS << " (Entry)";

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

    ElevatedCFGBlock *EB = BlockMap[CurrBlock];

    if (PrintOption == 1) {
      // Print the In set for the block.
      OS << "\n  In:\n";
      PrintBoundsMap(EB->In, PrintOption);

      // Print the Out set for the block.
      OS << "  Out:\n";
      PrintBoundsMap(EB->Out, PrintOption);
    }

    if (CurrBlock->empty()) {
      OS << "\n";
      continue;
    }

    for (CFGElement Elem : *CurrBlock) {
      if (Elem.getKind() != CFGElement::Statement)
        continue;
      const Stmt *CurrStmt = Elem.castAs<CFGStmt>().getStmt();
      if (!CurrStmt)
        continue;
   
      if (PrintOption == 0) {
        OS << "\n  Widened bounds before stmt: ";
      } else if (PrintOption == 1) {
        OS << "\n  Stmt: ";
      }

      // Print the current statement.
      PrintStmt(CurrStmt);

      if (PrintOption == 0) {
        // Print widened bounds before the statement.
        PrintBoundsMap(GetStmtIn(CurrBlock, CurrStmt), PrintOption);

      } else if (PrintOption == 1) {
        // Print the In set for the statement.
        OS << "  In:\n";
        PrintBoundsMap(GetStmtIn(CurrBlock, CurrStmt), PrintOption);

        // Print the Gen set for the statement.
        OS << "  Gen:\n";
        PrintBoundsMap(EB->StmtGen[CurrStmt], PrintOption);

        // Print the Kill set for the statement.
        OS << "  Kill:\n";
        PrintVarSet(EB->StmtKill[CurrStmt], PrintOption);

        // Print the Out set for the statement.
        OS << "  Out:\n";
        PrintBoundsMap(GetStmtOut(CurrBlock, CurrStmt), PrintOption);
      }
    }
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

bool BoundsWideningUtil::IsSwitchCaseBlock(const CFGBlock *CurrBlock,
                                           const CFGBlock *PredBlock) const {
  // Check if PredBlock ends in a switch statement.
  const Stmt *TerminatorStmt = PredBlock->getTerminatorStmt();
  if (TerminatorStmt && isa<SwitchStmt>(TerminatorStmt)) {
    // Get the SwitchStmt and check if it contains errors
    const auto *SSCond =
               (dyn_cast_or_null<SwitchStmt>(TerminatorStmt))->getCond();
    if (SSCond->containsErrors())
      return false;
  } else
    return false;

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

void BoundsWideningUtil::GetPtrsWithVarsInLowerBounds(
  VarSetTy &Vars, VarSetTy &PtrsWithVarsInLowerBounds) const {

  // Get the set of variables that are pointers to null-terminated arrays and
  // in whose lower bounds expressions the variables in Vars occur.

  for (const VarDecl *V : Vars) {
    auto VarPtrIt = BoundsVarsLower.find(V);
    if (VarPtrIt != BoundsVarsLower.end()) {
      for (const VarDecl *Ptr : VarPtrIt->second)
        if (!Ptr->isInvalidDecl() && IsNtArrayType(Ptr))
          PtrsWithVarsInLowerBounds.insert(Ptr);
    }
  }
}

void BoundsWideningUtil::GetPtrsWithVarsInUpperBounds(
  VarSetTy &Vars, VarSetTy &PtrsWithVarsInUpperBounds) const {

  // Get the set of variables that are pointers to null-terminated arrays and
  // in whose upper bounds expressions the variables in Vars occur.

  for (const VarDecl *V : Vars) {
    auto VarPtrIt = BoundsVarsUpper.find(V);
    if (VarPtrIt != BoundsVarsUpper.end()) {
      for (const VarDecl *Ptr : VarPtrIt->second)
        if (!Ptr->isInvalidDecl() && IsNtArrayType(Ptr))
          PtrsWithVarsInUpperBounds.insert(Ptr);
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

TermCondInfoTy BoundsWideningUtil::GetTermCondInfo(
  const Expr *TermCond) const {

  TermCondInfoTy TermCondInfo;
  // Initialize fields of TermCondInfo.
  TermCondInfo.DerefExpr = nullptr;
  TermCondInfo.IsCheckNull = false;

  FillTermCondInfo(TermCond, TermCondInfo);
  return TermCondInfo;
}

void BoundsWideningUtil::FillTermCondInfo(const Expr *TermCond,
                                          TermCondInfoTy &TermCondInfo) const {
  Expr *E = StripCasts(TermCond);

  // *p, !*p, !(*p == 0), etc.
  if (auto *UO = dyn_cast<UnaryOperator>(E)) {
    UnaryOperatorKind UnaryOp = UO->getOpcode();
    Expr *SubExpr = IgnoreCasts(UO->getSubExpr());

    // *p, LHS of *p == 0, etc.
    if (UnaryOp == UO_Deref) {
      // We do the following normalizations:
      // *p ==> *p != 0
      // !*p ==> *p == 0
      // In the above normalizations the compared value would always be null.
      // Whether the condition tests for a null value will be determined in
      // the logic for UO_LNot after we return from the current recursive
      // call.

      // *p ==> DerefExpr = p
      // *(p + 1) ==> DerefExpr = p + 1
      // **p ==> DerefExpr = *p
      TermCondInfo.DerefExpr = SubExpr;

      // !*p, !!!*p, !(*p == 0), etc.
    } else if (UnaryOp == UO_LNot) {
      // !*p ==> FillTermCondInfo(*p);
      // !!*p ==> FillTermCondInfo(!*p);
      // !!!*p ==> FillTermCondInfo(!!*p);
      // !(*p == 0) ==> FillTermCondInfo(*p == 0);
      FillTermCondInfo(SubExpr, TermCondInfo);

      // *p ==> IsCheckNull = False
      // !*p ==> IsCheckNull = True
      // !!*p ==> IsCheckNull = False
      // !!!*p ==> IsCheckNull = True
      TermCondInfo.IsCheckNull = !TermCondInfo.IsCheckNull;
    }

    // If dereference expression contains an array access. An array access can
    // be written A[i] or i[A] (both are equivalent).  getBase() and getIdx()
    // always present the normalized view: A[i]. In this case getBase() returns
    // "A" and getIdx() returns "i".
  } else if (auto *AE = dyn_cast<ArraySubscriptExpr>(E)) {
    // We do the following normalizations:
    // p[i] ==> p[i] != 0
    // !p[i] ==> p[i] == 0
    // In the above normalizations the compared value would always be null.
    // Whether the condition tests for a null value will be determined in
    // the logic for UO_LNot after we return from the current recursive
    // call.

    // p[i] ==> DerefExpr = p + i
    TermCondInfo.DerefExpr =
      ExprCreatorUtil::CreateBinaryOperator(SemaRef, AE->getBase(),
                                            AE->getIdx(),
                                            BinaryOperatorKind::BO_Add);

  } else if (auto *BO = dyn_cast<BinaryOperator>(E)) {
    BinaryOperatorKind BinOp = BO->getOpcode();

    // (c = *p) != 0
    if (BinOp == BO_Assign) {
      // (c = *p) ==> RHS = *p
      Expr *RHS = BO->getRHS();
      FillTermCondInfo(RHS, TermCondInfo);
      return;
    }

    // We only handle *p == 0, 0 != *p, etc.
    if (BinOp != BO_EQ && BinOp != BO_NE)
      return;

    Expr *LHS = BO->getLHS();
    Expr *RHS = BO->getRHS();

    if (LHS->containsErrors() || RHS->containsErrors())
      return;

    // 0 == *p ==> LHSVal = 0
    // 'a' != *p ==> LHSVal = 'a'
    // a != *p ==> LHSVal = nullptr
    // *p == *q ==> LHSVal = nullptr;
    Optional<llvm::APSInt> LHSVal = LHS->getIntegerConstantExpr(Ctx);

    // *p == 0 ==> RHSVal = 0
    // *p != 'a' ==> RHSVal = 'a'
    // *p != a ==> RHSVal = nullptr
    // *p == *q ==> RHSVal = nullptr
    Optional<llvm::APSInt> RHSVal = RHS->getIntegerConstantExpr(Ctx);

    // 1 == 2 ==> DerefExpr = nullptr
    // *p != q ==> DerefExpr = nullptr
    if ((LHSVal && RHSVal) || (!LHSVal && !RHSVal))
      return;

    bool IsComparedValNull = false;

    llvm::APSInt Zero(Ctx.getTargetInfo().getIntWidth(), 0);
    if (LHSVal) {
      // 0 == *p ==> IsComparedValNull = True
      // 'a' != *p ==> IsComparedValNull = False
      IsComparedValNull =
        llvm::APSInt::compareValues(*LHSVal, Zero) == 0;

      // 0 == *p ==> FillTermCondInfo(*p);
      // 'a' != *p ==> FillTermCondInfo(*p);
      FillTermCondInfo(RHS, TermCondInfo);

    } else if (RHSVal) {
      // *p == 0 ==> IsComparedValNull = True
      // *p != 'a' ==> IsComparedValNull = False
      IsComparedValNull =
        llvm::APSInt::compareValues(*RHSVal, Zero) == 0;

      // *p == 0 ==> FillTermCondInfo(*p);
      // *p != 'a' ==> FillTermCondInfo(*p);
      FillTermCondInfo(LHS, TermCondInfo);
    }

    // *p == 0  ==> BinOp == BO_EQ, IsComparedValNull = True,
    //              IsCheckNull(prev value) = False
    //          ==> IsCheckNull = True

    // *p != 'a' ==> BinOp != BO_EQ, IsComparedValNull = False,
    //               IsCheckNull(prev value) = False
    //           ==> IsCheckNull = True
    TermCondInfo.IsCheckNull = (BinOp == BO_EQ && IsComparedValNull &&
                                 !TermCondInfo.IsCheckNull) ||
                               (BinOp != BO_EQ && !IsComparedValNull &&
                                 !TermCondInfo.IsCheckNull);
  }
}

const VarDecl *BoundsWideningUtil::GetNullTermPtrInExpr(Expr *E) const {
  // Get the variable in an expression that is a pointer to a null-terminated
  // array.
  // Note: We assume that an expression can only contain at most one pointer to
  // a null-terminated array.

  if (!E)
    return nullptr;

  if (auto *CE = dyn_cast<CastExpr>(E))
    if (CE->getCastKind() == CastKind::CK_LValueToRValue)
      E = CE->getSubExpr();

  E = IgnoreCasts(E);

  // Get the pointer to a null-terminated array in an expression like *e.
  if (const auto *UO = dyn_cast<const UnaryOperator>(E)) {
    return GetNullTermPtrInExpr(UO->getSubExpr());

  // Get the pointer to a null-terminated array in an expression like e1 + e2.
  } else if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    const VarDecl *V = GetNullTermPtrInExpr(BO->getLHS());
    if (V)
      return V;
    return GetNullTermPtrInExpr(BO->getRHS());
  }

  // If the variable is a pointer to a null-terminated array return it.
  if (const auto *D = dyn_cast<DeclRefExpr>(E)) {
    if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
      if (!V->isInvalidDecl() && IsNtArrayType(V))
        return V;
  }
  return nullptr;
}

Expr *BoundsWideningUtil::StripCasts(const Expr *TermCond) const {
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

  return IgnoreCasts(E);
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
