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

void BoundsWideningAnalysis::WidenBounds(FunctionDecl *FD,
                                         StmtSetTy NestedStmts) {
  assert(Cfg && "expected CFG to exist");

  WorkListTy WorkList;

  // Add each block to WorkList and create a mapping from CFGBlock to
  // ElevatedCFGBlock.
  // Note: By default, PostOrderCFGView iterates in reverse order. So we always
  // get a reverse post order when we iterate PostOrderCFGView.
  for (const CFGBlock *B : PostOrderCFGView(Cfg)) {
    // SkipBlock will skip all null blocks and the exit block. PostOrderCFGView
    // does not traverse any unreachable blocks. So at the end of this loop
    // BlockMap only contains reachable blocks.
    if (SkipBlock(B))
      continue;

    auto EB = new ElevatedCFGBlock(B);
    BlockMap[B] = EB;

    // Initialize the In and Out sets for the entry block to empty sets.
    if (B == &Cfg->getEntry()) {
      EB->In = BoundsMapTy();
      EB->Out = BoundsMapTy();
      continue;
    }

    // Note: WorkList is a queue. So we maintain the reverse post order when we
    // iterate WorkList.
    WorkList.append(EB);

    // Compute Gen and Kill sets for the block and statements in the block.
    ComputeGenKillSets(EB);

    // Initialize the In and Out sets for the block.
    InitBlockInOutSets(EB);
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

      ComputeStmtGenKillSets(EB, CurrStmt, /*IsLastStmtInBlock*/ I == E - 1);
      ComputeUnionGenKillSets(EB, CurrStmt, PrevStmt);
      UpdateNtPtrsInFunc(EB, CurrStmt);

      PrevStmt = CurrStmt;

      if (I == E - 2)
        EB->SecondLastStmt = CurrStmt;
      else if (I == E - 1)
        EB->LastStmt = CurrStmt;
    }
  }

  ComputeBlockGenKillSets(EB);
}

void BoundsWideningAnalysis::ComputeStmtGenKillSets(ElevatedCFGBlock *EB,
                                                    const Stmt *CurrStmt,
                                                    bool IsLastStmtInBlock) {
  // Initialize the Gen and Kill sets for the statement.
  EB->StmtGen[CurrStmt] = BoundsMapTy();
  EB->StmtKill[CurrStmt] = VarSetTy();

  // Determine whether CurrStmt generates a dataflow fact.

  // A conditional statement that dereferences a variable that is a pointer to
  // a null-terminated array can generate a dataflow fact. For example: if (*(p
  // + 1)) The conditional will always be the last statement in the block.
  if (IsLastStmtInBlock) {
    FillStmtGenKillSetsForPtrDeref(EB, CurrStmt);

  // A bounds declaration of a null-terminated array generates a dataflow fact.
  // For example: _Nt_array_ptr<char> p : bounds(p, p + 1);
  } else if (const auto *DS = dyn_cast<DeclStmt>(CurrStmt)) {
    for (const Decl *D : DS->decls()) {
      if (const auto *V = dyn_cast<VarDecl>(D)) {
        FillStmtGenKillSetsForBoundsDecl(EB, CurrStmt, V);

        // Additionally, a where clause on a declaration can generate a
        // dataflow fact.
        // For example: int x = strlen(p) _Where p : bounds(p, p + x);
        FillStmtGenKillSetsForWhereClause(EB, CurrStmt, V->getWhereClause());
      }
    }

  // A where clause on an expression statement (which is represented in the
  // AST as a ValueStmt) can generate a dataflow fact.
  // For example: x = strlen(p) _Where p : bounds(p, p + x);
  } else if (const auto *VS = dyn_cast<ValueStmt>(CurrStmt)) {
    FillStmtGenKillSetsForWhereClause(EB, CurrStmt, VS->getWhereClause());

  // A where clause on a null statement (meaning a standalone where clause) can
  // generate a dataflow fact.
  // For example: _Where p : bounds(p, p + 1);
  } else if (const auto *NS = dyn_cast<NullStmt>(CurrStmt)) {
    FillStmtGenKillSetsForWhereClause(EB, CurrStmt, NS->getWhereClause());
  }

  // If a variable modified by CurrStmt occurs in the bounds expression of a
  // null-terminated array then the bounds of that null-terminated array should
  // be killed.
  FillStmtKillSetForModifiedVars(EB, CurrStmt);
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

  EB->UnionKill[CurrStmt] = Union(EB->UnionKill[PrevStmt],
                                  EB->StmtKill[CurrStmt]);

  auto Diff = Difference(EB->UnionGen[PrevStmt], EB->UnionKill[CurrStmt]);
  EB->UnionGen[CurrStmt] = Union(Diff, EB->StmtGen[CurrStmt]);
}

void BoundsWideningAnalysis::ComputeBlockGenKillSets(ElevatedCFGBlock *EB) {
  if (!EB->LastStmt)
    return;

  EB->Gen = EB->UnionGen[EB->LastStmt];
  EB->Kill = EB->UnionKill[EB->LastStmt];
}

void BoundsWideningAnalysis::ComputeInSet(ElevatedCFGBlock *EB) {
  // Iterate through all preds of EB.
  for (const CFGBlock *pred : EB->Block->preds()) {
    if (SkipBlock(pred))
      continue;

    ElevatedCFGBlock *PredEB = BlockMap[pred];
    auto PredOut = PredEB->Out;

    BoundsMapTy GenOfLastStmt = PredEB->StmtGen[PredEB->LastStmt];
    BoundsMapTy StmtInOfLastStmt = GetStmtOut(PredEB, PredEB->SecondLastStmt);

    for (auto Item : GenOfLastStmt) {
      const VarDecl *V = Item.first;
      RangeBoundsExpr *BoundsBeforeLastStmt = StmtInOfLastStmt[V];

      bool IsTrueEdge = IsEdgeTrue(pred, EB->Block);
      bool IsDerefAtUpperBound =
        Lex.CompareExpr(PredEB->TermCondDerefExpr,
                        BoundsBeforeLastStmt->getUpperExpr()) ==
        Lexicographic::Result::Equal;

      // If the edge from pred to current block is not a true edge then we
      // cannot widen the bounds upon entry to the current block. So we reset
      // the bounds to those before the last statement in the pred block.
      if (!IsTrueEdge) {
        PredOut[V] = BoundsBeforeLastStmt;

      // Else if the terminating condition of the pred block does not V at the
      // current upper bound, then we cannot widen the bounds upon entry to the
      // current block. So we reset the bounds of V to those before the last
      // statement in PredEB.
      } else if (!IsDerefAtUpperBound) {
        PredOut[V] = BoundsBeforeLastStmt;
      }
    }

    EB->In = Intersect(EB->In, PredOut);
  }
}

bool BoundsWideningAnalysis::IsEdgeTrue(const CFGBlock *PredBlock,
                                        const CFGBlock *CurrBlock) {
  // Is the edge from PredBlock to CurrBlock a true edge?
  // A true edge is never the last edge in the list of outgoing edges of a
  // block.
  // TODO: The above may not be true for switch cases. Need to handle it.
  return CurrBlock != *(PredBlock->succs().end() - 1);
}

void BoundsWideningAnalysis::ComputeOutSet(ElevatedCFGBlock *EB,
                                           WorkListTy &WorkList) {
  auto OrigOut = EB->Out;

  auto Diff = Difference(EB->In, EB->Kill);
  EB->Out = Union(Diff, EB->Gen);

  if (!IsEqual(EB->Out, OrigOut))
    WorkList.append(EB);
}

void BoundsWideningAnalysis::InitBlockInOutSets(ElevatedCFGBlock *EB) {
  // Initialize the In and Out sets of block EB to Top. This function is called
  // for all blocks except the entry block.
  for (const VarDecl *V : AllNtPtrsInFunc) {
    EB->In[V] = Top;
    EB->Out[V] = Top;
  }
}

BoundsMapTy BoundsWideningAnalysis::GetStmtOut(ElevatedCFGBlock *EB,
                                               const Stmt *CurrStmt) {
  if (CurrStmt) {
    auto Diff = Difference(EB->In, EB->UnionKill[CurrStmt]);
    return Union(Diff, EB->UnionGen[CurrStmt]);
  }
  return EB->In;
}

void BoundsWideningAnalysis::UpdateNtPtrsInFunc(ElevatedCFGBlock *EB,
                                                const Stmt *CurrStmt) {
  // Store all variables that are pointers to null-terminated arrays in
  // AllVarsInFunc. These are the variables that are in StmtGen for all
  // statements in all blocks of the function.
  for (auto Item : EB->StmtGen[CurrStmt])
    AllNtPtrsInFunc.insert(Item.first);
}

void BoundsWideningAnalysis::FillStmtGenKillSetsForBoundsDecl(
  ElevatedCFGBlock *EB, const Stmt *CurrStmt, const VarDecl *V) {

  if (IsNtArrayType(V) && V->hasBoundsExpr()) {
    BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(V);
    EB->StmtGen[CurrStmt][V] = dyn_cast<RangeBoundsExpr>(NormalizedBounds);
    EB->StmtKill[CurrStmt].insert(V);
  }
}

void BoundsWideningAnalysis::FillStmtGenKillSetsForWhereClause(
  ElevatedCFGBlock *EB, const Stmt *CurrStmt, WhereClause *WC) {

  if (!WC)
    return;

  for (const auto *Fact : WC->getFacts()) {
    auto *BF = dyn_cast<BoundsDeclFact>(Fact);
    if (!BF)
      continue;

    VarDecl *V = BF->Var;
    if (IsNtArrayType(V)) {
      BoundsExpr *NormalizedBounds = SemaRef.ExpandBoundsToRange(V, BF->Bounds);
      EB->StmtGen[CurrStmt][V] = dyn_cast<RangeBoundsExpr>(NormalizedBounds);
      EB->StmtKill[CurrStmt].insert(V);
    }
  }
}

void BoundsWideningAnalysis::FillStmtGenKillSetsForPtrDeref(
  ElevatedCFGBlock *EB, const Stmt *CurrStmt) {

  // Get the terminating condition of the block.
  Expr *TermCond = GetTerminatorCondition(EB->Block);
  if (!TermCond)
    return;

  // If the terminating condition is a dereference expression get that
  // expression.
  Expr *DerefExpr = GetDerefExpr(TermCond);
  if (!DerefExpr)
    return;

  EB->TermCondDerefExpr = DerefExpr;

  // Get the set of null-terminated arrays that the dereference expression
  // potentially widens. A dereference expression can only widen the bounds of
  // those null-terminated arrays whose upper bounds expression contains all
  // variables that occur in the dereference expression. For example: *(p + i +
  // j) can widen p iff p, i and j all occur in upper_bound(p).
  VarSetTy VarsToWiden;
  GetVarsToWiden(DerefExpr, VarsToWiden);

  RangeBoundsExpr *R = nullptr;
  for (const VarDecl *V : VarsToWiden) {
    if (!IsNtArrayType(V))
      continue;

    if (!R) {
      BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(V);
      RangeBoundsExpr *RBE = dyn_cast<RangeBoundsExpr>(NormalizedBounds);

      // DerefExpr potentially widens V. So we add
      // "V:bounds(lower, DerefExpr + 1)" to the Gen set.
      R = new (Ctx) RangeBoundsExpr(RBE->getLowerExpr(),
                                    GetWidenedExpr(DerefExpr, 1),
                                    SourceLocation(), SourceLocation());
    }

    EB->StmtGen[CurrStmt][V] = R;
    EB->StmtKill[CurrStmt].insert(V);
  }
}

void BoundsWideningAnalysis::FillStmtKillSetForModifiedVars(
  ElevatedCFGBlock *EB, const Stmt *CurrStmt) {

  // Get variables modified by CurrStmt or statements nested in CurrStmt.
  VarSetTy ModifiedVars;
  GetModifiedVars(CurrStmt, ModifiedVars);

  // If a modified variable occurs in the lower or upper bounds expression of a
  // null-terminated array then the bounds of that null-terminated array should
  // be killed.
  for (const VarDecl *ModifiedVar : ModifiedVars) {
    auto It = BoundsVarsLower.find(ModifiedVar);
    if (It != BoundsVarsLower.end()) {
      for (const VarDecl *V : It->second)
        EB->StmtKill[CurrStmt].insert(V);
    }

    It = BoundsVarsUpper.find(ModifiedVar);
    if (It != BoundsVarsUpper.end()) {
      for (const VarDecl *V : It->second)
        EB->StmtKill[CurrStmt].insert(V);
    }
  }
}


void BoundsWideningAnalysis::GetVarsToWiden(Expr *E, VarSetTy &VarsToWiden) {
  // Determine the set of variables that can be widened in an expression.
  if (!E)
    return;

  // Get all variables that occur in the expression.
  VarSetTy VarsInExpr;
  GetVarsInExpr(E, VarsInExpr);

  // If we have the following declarations:
  // _Nt_array_ptr<T> p : bounds(p + i, p + x + y);
  // _Nt_array_ptr<T> q : bounds(q + j, q + x + z);
  // Then BoundsVarsUpper contains the following entries:
  // p : {p}
  // q : {q}
  // x : {p, q}
  // y : {p}
  // z : {q}
  VarSetTy PtrsWithVarInUpperBounds;
  for (const VarDecl *V : VarsInExpr) {
    // Get the set of null-terminated arrays in whose upper bounds expressions
    // V occurs.
    auto It = BoundsVarsUpper.find(V);

    // If V does not appear in the upper bounds expression of any
    // null-terminated array then expression E cannot potentially widen the
    // bounds of any null-terminated array.
    if (It == BoundsVarsUpper.end())
      return;

    if (PtrsWithVarInUpperBounds.empty())
      PtrsWithVarInUpperBounds = It->second;
    else
      PtrsWithVarInUpperBounds = Intersect(PtrsWithVarInUpperBounds,
                                           It->second);

    // If the intersection of PtrsWithVarInUpperBounds is empty then expression
    // E cannot potentially widen the bounds of any null-terminated array.
    if (PtrsWithVarInUpperBounds.empty())
      return;
  }

  // Gather the set of variables occurring in the upper bounds expression of
  // each pointer.
  // If we have the following declarations:
  // _Nt_array_ptr<T> p : bounds(p + i, p + x + y);
  // _Nt_array_ptr<T> q : bounds(q + j, q + x + z);
  // Then VarsInBounds would contain the following entries:
  // p : {p, x, y}
  // q : {q, x, z}
  BoundsVarsTy VarsInBounds;
  for (auto Pair : BoundsVarsUpper) {
    const VarDecl *V = Pair.first;
    for (const VarDecl *Ptr : Pair.second) {
      if (PtrsWithVarInUpperBounds.count(Ptr))
        VarsInBounds[Ptr].insert(V);
    }
  }

  // If the number of variables occurring in the upper bounds expression of
  // each pointer in PtrsWithVarInUpperBounds is equal to the number of
  // variables occurring in the dereference expression then that pointer can
  // potentially be widened.
  for (const VarDecl *Ptr : PtrsWithVarInUpperBounds) {
    if (VarsInExpr.size() == VarsInBounds[Ptr].size())
      VarsToWiden.insert(Ptr);
  }
}

void BoundsWideningAnalysis::GetModifiedVars(const Stmt *CurrStmt,
                                             VarSetTy &ModifiedVars) {
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

Expr *BoundsWideningAnalysis::GetWidenedExpr(Expr *E, unsigned Offset) const {
  // CopyAurns the expression E + Offset.
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

Expr *BoundsWideningAnalysis::GetTerminatorCondition(const Expr *E) const {
  if (!E)
    return nullptr;

  if (const auto *BO = dyn_cast<BinaryOperator>(E->IgnoreParens()))
    return GetTerminatorCondition(BO->getRHS());

  // According to C11 standard section 6.5.13, the logical AND Operator shall
  // yield 1 if both of its operands compare unequal to 0; otherwise, it yields
  // 0. The result has type int.  An IntegralCast is generated for "if (*p &&
  // *(p + 1))", where p is _Nt_array_ptr<T>.  Here we strip off the
  // IntegralCast.
  if (auto *CE = dyn_cast<CastExpr>(E))
    if (CE->getCastKind() == CastKind::CK_IntegralCast)
      return const_cast<Expr *>(CE->getSubExpr());
  return const_cast<Expr *>(E);
}

Expr *BoundsWideningAnalysis::GetTerminatorCondition(const CFGBlock *B) const {
  if (!B)
    return nullptr;

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

Expr *BoundsWideningAnalysis::GetDerefExpr(Expr *E) const {
  // A dereference expression can contain an array subscript or a pointer
  // dereference.
  if (!E)
    return nullptr;

  if (auto *CE = dyn_cast<CastExpr>(E))
    if (CE->getCastKind() == CastKind::CK_LValueToRValue)
      E = CE->getSubExpr();

  E = IgnoreCasts(E);

  // If a dereference expression is of the form "*(p + i)".
  if (auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() == UO_Deref)
      return IgnoreCasts(UO->getSubExpr());

  // Else if a dereference expression is an array access. An array access can
  // be written A[i] or i[A] (both are equivalent).  getBase() and getIdx()
  // always present the normalized view: A[i]. In this case getBase() returns
  // "A" and getIdx() returns "i".

  // TODO: Currently we normalize A[i] to "A + i" and return that as the
  // dereference expression because we need to extract the variables that occur
  // in the deref expression in the function GetVarsInExpr. But once we change
  // this analysis to use AbstractSets we no longer need to normalize the
  // expression here and can simply return the ArraySubscriptExpr from this
  // function.
  } else if (auto *AE = dyn_cast<ArraySubscriptExpr>(E)) {
    return ExprCreatorUtil::CreateBinaryOperator(SemaRef, AE->getBase(),
                                                 AE->getIdx(),
                                                 BinaryOperatorKind::BO_Add);
  }
  return nullptr;
}

void BoundsWideningAnalysis::GetVarsInExpr(Expr *E,
                                           VarSetTy &VarsInExpr) const {
  // Get the VarDecls of all variables occurring in an expression.

  // TODO: Currently this function returns a subset of all variables that occur
  // in an expression. It returns only the top-level variables that appear in
  // an expression. For example, in the expression "a + b - c" it returns {a,
  // b, c}. It does not return variables that are members of a struct or
  // arguments of a function call, etc.
  // In future, when we change this analysis to use AbstractSets, this function
  // can handle an expression like "s->f + func(x)" and return {s, f, x} as the
  // set of variables that occur in the expression.

  if (!E)
    return;

  if (auto *CE = dyn_cast<CastExpr>(E))
    if (CE->getCastKind() == CastKind::CK_LValueToRValue)
      E = CE->getSubExpr();

  E = IgnoreCasts(E);

  // Get variables in an expression like *e.
  if (const auto *UO = dyn_cast<const UnaryOperator>(E)) {
      GetVarsInExpr(UO->getSubExpr(), VarsInExpr);

  // Get variables in an expression like e1 + e2.
  } else if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    GetVarsInExpr(BO->getLHS(), VarsInExpr);
    GetVarsInExpr(BO->getRHS(), VarsInExpr);
  }

  if (const auto *D = dyn_cast<DeclRefExpr>(E)) {
    if (const auto *V = dyn_cast<VarDecl>(D->getDecl()))
      VarsInExpr.insert(V);
  }
}

Expr *BoundsWideningAnalysis::IgnoreCasts(const Expr *E) const {
  return Lex.IgnoreValuePreservingOperations(Ctx, const_cast<Expr *>(E));
}

bool BoundsWideningAnalysis::SkipBlock(const CFGBlock *B) const {
  return !B || B == &Cfg->getEntry() || B == &Cfg->getExit();
}

bool BoundsWideningAnalysis::IsNtArrayType(const VarDecl *V) const {
  return V && (V->getType()->isCheckedPointerNtArrayType() ||
               V->getType()->isNtCheckedArrayType());
}

void BoundsWideningAnalysis::DumpWidenedBounds(FunctionDecl *FD) {
  OS << "--------------------------------------\n";
  OS << "In function: " << FD->getName() << "\n";

  for (const CFGBlock *B : GetOrderedBlocks()) {
    OS << "======================================";
    B->print(OS, Cfg, SemaRef.getLangOpts(), /* ShowColors */ true);

    ElevatedCFGBlock *EB = BlockMap[B];

    for (CFGElement Elem : *B) {
      if (Elem.getKind() == CFGElement::Statement) {
        const Stmt *CurrStmt = Elem.castAs<CFGStmt>().getStmt();
        if (!CurrStmt)
          continue;

        BoundsMapTy WidenedBounds = GetStmtOut(EB, CurrStmt);

        std::vector<const VarDecl *> Vars;
        for (auto Item : WidenedBounds)
          Vars.push_back(Item.first);

        llvm::sort(Vars.begin(), Vars.end(),
          [](const VarDecl *A, const VarDecl *B) {
             return A->getQualifiedNameAsString().compare(
                    B->getQualifiedNameAsString()) < 0;
          });

        OS << "\n--------------------------------------\n";
        OS << "### At Stmt:\n";
        CurrStmt->dump(OS, Ctx);
        OS << "\n### Bounds are:\n";
        for (const VarDecl *V : Vars) {
          OS << V->getQualifiedNameAsString() << ": ";
          WidenedBounds[V]->dump();
        }
      }
    }
  }
}

OrderedBlocksTy BoundsWideningAnalysis::GetOrderedBlocks() {
  // We order the CFG blocks based on block ID. Block IDs decrease from entry
  // to exit. So we sort in the reverse order.
  OrderedBlocksTy OrderedBlocks;
  for (auto Item : BlockMap) {
    const CFGBlock *B = Item.first;
    OrderedBlocks.push_back(B);
  }

  llvm::sort(OrderedBlocks.begin(), OrderedBlocks.end(),
    [] (const CFGBlock *A, const CFGBlock *B) {
        return A->getBlockID() > B->getBlockID();
    });
  return OrderedBlocks;
}

// TODO: Move the templated (not specialized) set operation functions to a
// common header.

// Common templated set operation functions.
template<class T, class U>
T BoundsWideningAnalysis::Difference(T &A, U &B) const {
  if (!A.size() || !B.size())
    return A;

  auto CopyA = A;
  for (auto ItemA : A) {
    if (B.count(ItemA))
      CopyA.erase(ItemA);
  }
  return CopyA;
}

template<class T>
T BoundsWideningAnalysis::Union(T &A, T &B) const {
  auto CopyA = A;
  for (auto ItemB : B)
    CopyA.insert(ItemB);

  return CopyA;
}

template<class T>
T BoundsWideningAnalysis::Intersect(T &A, T &B) const {
  if (!A.size() || !B.size())
    return T();

  auto CopyA = A;
  for (auto ItemA : A) {
    if (!B.count(ItemA))
      CopyA.erase(ItemA);
  }
  return CopyA;
}

template<class T>
bool BoundsWideningAnalysis::IsEqual(T &A, T &B) const {
  return A.size() == B.size() &&
         A.size() == Intersect(A, B).size();
}

// Template specializations of common set operation functions.
template<>
BoundsMapTy BoundsWideningAnalysis::Difference<BoundsMapTy, VarSetTy>(
  BoundsMapTy &A, VarSetTy &B) const {

  if (!A.size() || !B.size())
    return A;

  auto CopyA = A;
  for (auto ItemA : A) {
    if (B.count(ItemA.first))
      CopyA.erase(ItemA.first);
  }
  return CopyA;
}

template<>
BoundsMapTy BoundsWideningAnalysis::Intersect<BoundsMapTy>(
  BoundsMapTy &A, BoundsMapTy &B) const {

  if (!A.size() || !B.size())
    return A;

  auto CopyA = A;
  for (auto ItemB : B) {
    const VarDecl *V = ItemB.first;
    auto IterA = CopyA.find(V);
    if (IterA == CopyA.end()) {
      CopyA.erase(V);
      continue;
    }

    RangeBoundsExpr *BoundsCopyA = IterA->second;
    RangeBoundsExpr *BoundsB = ItemB.second;

    if (BoundsCopyA == Top ||
        SemaRef.IsSubRange(BoundsCopyA, BoundsB))
      CopyA[V] = BoundsB;
  }
  return CopyA;
}

template<>
BoundsMapTy BoundsWideningAnalysis::Union<BoundsMapTy>(
  BoundsMapTy &A, BoundsMapTy &B) const {

  auto CopyA = A;
  for (auto ItemB : B)
    CopyA[ItemB.first] = ItemB.second;

  return CopyA;
}

template<>
bool BoundsWideningAnalysis::IsEqual<BoundsMapTy>(
  BoundsMapTy &A, BoundsMapTy &B) const {

  if (A.size() != B.size())
    return false;

  auto CopyA = A;
  for (auto ItemB : B) {
    const VarDecl *V = ItemB.first;

    auto IterA = CopyA.find(V);
    if (IterA == CopyA.end())
      return false;

    RangeBoundsExpr *BoundsA = IterA->second;
    RangeBoundsExpr *BoundsB = ItemB.second;

    if (BoundsA == Top || BoundsB == Top)
      return BoundsA == BoundsB;

    if (Lex.CompareExpr(BoundsA->getUpperExpr(), BoundsB->getUpperExpr()) !=
        Lexicographic::Result::Equal)
      return false;
  }
  return true;
}

} // end namespace clang
