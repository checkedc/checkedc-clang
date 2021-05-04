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

    // Compute Gen and Kill sets for statements in the block.
    ComputeGenKillSets(EB);
  }
}

void BoundsWideningAnalysis::ComputeGenKillSets(ElevatedCFGBlock *EB) {
  // Compute Gen and Kill sets for each statement in a block.
  const Stmt *PrevS = nullptr;

  for (auto I = EB->Block->begin(), E = EB->Block->end(); I != E; ++I) {
    CFGElement Elem = *I;
    if (Elem.getKind() == CFGElement::Statement) {
      const Stmt *S = Elem.castAs<CFGStmt>().getStmt();
      if (!S)
        continue;

      bool IsLastStmt = I == E - 1;
      ComputeGenSet(EB, S, PrevS, IsLastStmt);

      // TODO: Compute Kill sets only for top-level statements that are not
      // nested in another top-level statement.
      ComputeKillSet(EB, S, PrevS);

      PrevS = S;
    }
  }
}

void BoundsWideningAnalysis::ComputeGenSet(ElevatedCFGBlock *EB,
                                           const Stmt *S,
                                           const Stmt *PrevS,
                                           bool IsLastStmt) {
  // If this is the first statement in the block then initialize the Gen set to
  // empty, else initialize it to the Gen set of the previous statement minus
  // the Kill set for the previous statement.
  EB->Gen[S] = !PrevS ? BoundsMapTy() :
                Difference(EB->Gen[PrevS], EB->Kill[PrevS]);

  // Determine whether statement S generates a dataflow fact.
  if (const auto *DS = dyn_cast<DeclStmt>(S)) {
    for (const Decl *D : DS->decls()) {
      if (const auto *V = dyn_cast<VarDecl>(D)) {
        // A bounds declaration of a null-terminated array generates a
        // dataflow fact. For example:
        // _Nt_array_ptr<char> p : bounds(p, p + 1);
        FillGenSetForBoundsDecl(EB, S, V);

        // A where clause on a declaration can generate a dataflow fact.
        // For example: int x = strlen(p) _Where p : bounds(p, p + x);
        FillGenSetForWhereClause(EB, S, V->getWhereClause());
      }
    }

  // A where clause on an expression statement (which is represented in the
  // AST as a ValueStmt) can generate a dataflow fact.
  // For example: x = strlen(p) _Where p : bounds(p, p + x);
  } else if (const auto *VS = dyn_cast<ValueStmt>(S)) {
    FillGenSetForWhereClause(EB, S, VS->getWhereClause());

  // A conditional statement that dereferences a null-terminated array
  // variable can generate a dataflow fact. For example: if (*(p + 1))
  // The conditional will always be the last statement in the block.
  } else if (IsLastStmt) {
    FillGenSetForPtrDeref(EB, S);
  }
}

void BoundsWideningAnalysis::FillGenSetForBoundsDecl(ElevatedCFGBlock *EB,
                                                     const Stmt *S,
                                                     const VarDecl *V) {
  if (IsNtArrayType(V) && V->hasBoundsExpr())
    EB->Gen[S][V] = SemaRef.NormalizeBounds(V);
}

void BoundsWideningAnalysis::FillGenSetForWhereClause(ElevatedCFGBlock *EB,
                                                      const Stmt *S,
                                                      WhereClause *WC) {
  if (!WC)
    return;

  for (const auto *Fact : WC->getFacts()) {
    auto *BF = dyn_cast<BoundsDeclFact>(Fact);
    if (!BF)
      continue;

    VarDecl *V = BF->Var;
    if (IsNtArrayType(V))
      EB->Gen[S][V] = SemaRef.ExpandBoundsToRange(V, BF->Bounds);
  }
}

void BoundsWideningAnalysis::FillGenSetForPtrDeref(ElevatedCFGBlock *EB,
                                                   const Stmt *S) {
  // Get the terminating condition of the block.
  Expr *TermCond = GetTerminatorCondition(EB->Block);
  if (!TermCond)
    return;

  // If the terminating condition is a dereference expression get that
  // expression.
  Expr *DerefExpr = GetDerefExpr(TermCond);
  if (!DerefExpr)
    return;

  // Get all variables that occur in the dereference expression.
  VarSetTy VarsInExpr;
  GetVarsInExpr(DerefExpr, VarsInExpr);

  // The current dereference expression can only widen the bounds of those
  // null-terminated array variables whose bounds expressions contain all
  // variables Z that appear in VarsInExpr.
  // For example: *(p + i + j) can widen p iff p, i and j all occur in
  // bounds(p).

  // For each variable Z occurring in the dereference expression, lookup in
  // BoundsVars for the null-terminated array variables in whose bounds
  // expressions Z occurs. A deference expression involving Z may potentially
  // widen all such null-terminated array variables.
  VarSetTy VarsToWiden;
  for (const VarDecl *V : VarsInExpr) {
    auto It = BoundsVars.find(V);

    // If a variable does not appear in the bounds expressions of any other
    // variable then the current dereference expression cannot potentially
    // widen the bounds of any null-terminated array.
    if (It == BoundsVars.end())
      return;

    if (VarsToWiden.empty())
      VarsToWiden = It->second;
    else
      VarsToWiden = Intersect(VarsToWiden, It->second);

    // If the intersection is empty then the current dereference expression
    // cannot potentially widen the bounds of any null-terminated array.
    if (VarsToWiden.empty())
      return;
  }

  RangeBoundsExpr *R = nullptr;
  for (const VarDecl *V : VarsToWiden) {
    if (!IsNtArrayType(V))
      continue;

    if (!R) {
      BoundsExpr *Bounds = SemaRef.NormalizeBounds(V);
      RangeBoundsExpr *RBE = dyn_cast<RangeBoundsExpr>(Bounds);
      assert(RBE && "invalid bounds for null-terminated array");

      // DerefExpr potentially widens V. So we add
      // "V:bounds(lower, DerefExpr + 1)" to the Gen set.
      R = new (Ctx) RangeBoundsExpr(RBE->getLowerExpr(),
                                    GetWidenedExpr(DerefExpr, 1),
                                    SourceLocation(), SourceLocation());
    }

    EB->Gen[S][V] = R;
  }
}

void BoundsWideningAnalysis::ComputeKillSet(ElevatedCFGBlock *EB,
                                            const Stmt *S,
                                            const Stmt *PrevS) {
  // If this is the first statement in the block then initialize its Kill set
  // to empty, else initialize it to the Kill set of the previous statement.
  EB->Kill[S] = !PrevS ? VarSetTy() : EB->Kill[PrevS];

  // If a variable is added to the Gen set it means that all previous bounds of
  // that variable are killed. So also add that variable to the Kill set.
  for (const auto item : EB->Gen[S]) {
    const VarDecl *V = item.first;
    EB->Kill[S].insert(V);
  }

  // Get variables modified by statement S or statements nested in S.
  VarSetTy ModifiedVars;
  GetModifiedVars(S, ModifiedVars);

  // If a modified variable occurs in the bounds expression of a
  // null-terminated array then the bounds of that null-terminated array should
  // be killed.
  for (const VarDecl *ModifiedVar : ModifiedVars) {
    auto It = BoundsVars.find(ModifiedVar);
    if (It == BoundsVars.end())
      continue;

    for (const VarDecl *V : It->second)
      EB->Kill[S].insert(V);
  }
}

void BoundsWideningAnalysis::GetModifiedVars(const Stmt *S,
                                             VarSetTy &ModifiedVars) {
  // Get all variables modfied by statement S or statements nested in S.
  if (!S)
    return;

  Expr *E = nullptr;

  // If the variable is modified using a unary operator, like ++I or I++.
  if (const auto *UO = dyn_cast<const UnaryOperator>(S)) {
    if (UO->isIncrementDecrementOp()) {
      assert(UO->getSubExpr() && "invalid UnaryOperator expression");
      E = IgnoreCasts(UO->getSubExpr());
    }

  // Else if the variable is being assigned to, like I = ...
  } else if (const auto *BO = dyn_cast<const BinaryOperator>(S)) {
    if (BO->isAssignmentOp())
      E = IgnoreCasts(BO->getLHS());
  }

  if (const auto *D = dyn_cast_or_null<DeclRefExpr>(E))
    if (const auto *V = dyn_cast_or_null<VarDecl>(D->getDecl()))
      ModifiedVars.insert(V);

  for (const Stmt *NestedS : S->children())
    GetModifiedVars(NestedS, ModifiedVars);
}

Expr *BoundsWideningAnalysis::GetWidenedExpr(Expr *E, unsigned Offset) const {
  // Returns the expression E + Offset.

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
  // "A" and getIdx() returns "i". We normalize A[i] to "A + i" and return that
  // as the dereference expression.
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
  if (!E)
    return;

  E = IgnoreCasts(E);

  if (const auto *UO = dyn_cast<const UnaryOperator>(E)) {
      GetVarsInExpr(UO->getSubExpr(), VarsInExpr);

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
  return !B || B == &Cfg->getExit();
}

bool BoundsWideningAnalysis::IsNtArrayType(const VarDecl *V) const {
  return V && (V->getType()->isCheckedPointerNtArrayType() ||
               V->getType()->isNtCheckedArrayType());
}

template<class T, class U>
T BoundsWideningAnalysis::Difference(T &A, U &B) const {
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
T BoundsWideningAnalysis::Intersect(T &A, T &B) const {
  if (!A.size())
    return A;

  auto Ret = A;
  if (!B.size()) {
    Ret.clear();
    return Ret;
  }

  for (auto item : Ret) {
    if (!B.count(item))
      Ret.erase(item);
  }

  return Ret;
}

} // end namespace clang
