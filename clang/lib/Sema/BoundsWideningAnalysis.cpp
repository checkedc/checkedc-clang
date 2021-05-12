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

void
BoundsWideningAnalysis::WidenBounds(FunctionDecl *FD, StmtSetTy NestedStmts) {
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

    // Compute Gen and Kill sets for the block.
    ComputeGenKillSets(EB);
  }
}

void
BoundsWideningAnalysis::ComputeGenKillSets(ElevatedCFGBlock *EB) {
  // Compute Gen and Kill sets for the block.

  bool IsLastStmt = true;

  // Note: We iterate the statements in a block in reverse because it makes it
  // easier to compute the union of Kill sets for statement S_i+1 through
  // statement S_n.
  for (CFGBlock::const_reverse_iterator RI = EB->Block->rbegin(),
                                        RE = EB->Block->rend();
       RI != RE; ++RI) {
    CFGElement Elem = *RI;
    if (Elem.getKind() == CFGElement::Statement) {
      const Stmt *S = Elem.castAs<CFGStmt>().getStmt();
      if (!S)
        continue;

      ComputeStmtGenKillSets(EB, S, IsLastStmt);
      UpdateBlockGenKillSets(EB, S);

      IsLastStmt = false;
    }
  }
}

void
BoundsWideningAnalysis::ComputeStmtGenKillSets(ElevatedCFGBlock *EB,
                                               const Stmt *S,
                                               bool IsLastStmt) {
  // Initialize the Gen and Kill sets for the statement.
  EB->StmtGen[S] = BoundsMapTy();
  EB->StmtKill[S] = VarSetTy();

  // Determine whether statement S generates a dataflow fact.
  if (const auto *DS = dyn_cast<DeclStmt>(S)) {
    for (const Decl *D : DS->decls()) {
      if (const auto *V = dyn_cast<VarDecl>(D)) {
        // A bounds declaration of a null-terminated array generates a
        // dataflow fact. For example:
        // _Nt_array_ptr<char> p : bounds(p, p + 1);
        FillStmtGenKillSetsForBoundsDecl(EB, S, V);

        // A where clause on a declaration can generate a dataflow fact.
        // For example: int x = strlen(p) _Where p : bounds(p, p + x);
        FillStmtGenKillSetsForWhereClause(EB, S, V->getWhereClause());
      }
    }

  // A where clause on an expression statement (which is represented in the
  // AST as a ValueStmt) can generate a dataflow fact.
  // For example: x = strlen(p) _Where p : bounds(p, p + x);
  } else if (const auto *VS = dyn_cast<ValueStmt>(S)) {
    FillStmtGenKillSetsForWhereClause(EB, S, VS->getWhereClause());

  // A conditional statement that dereferences a variable that is a pointer to
  // a null-terminated array can generate a dataflow fact. For example: if (*(p
  // + 1)) The conditional will always be the last statement in the block.
  } else if (IsLastStmt) {
    FillStmtGenKillSetsForPtrDeref(EB, S);
  }

  // If a variable modified by statement S occurs in the bounds expression of a
  // null-terminated array then the bounds of that null-terminated array should
  // be killed.
  FillStmtKillSetForModifiedVars(EB, S);
}

void
BoundsWideningAnalysis::UpdateBlockGenKillSets(ElevatedCFGBlock *EB,
                                               const Stmt *S) {
  // Update the Gen set for the block. If we are currently processing statement
  // S_i in ComputeGenKillSets, then at this point EB->Kill is the union of the
  // Kill sets of statements S_i+1 through S_n. This is why we iterate the
  // statements in reverse in ComputeGenKillSets as it makes it easier to
  // compute the union of Kill sets of statements S_i+1 through S_n.
  BoundsMapTy Diff = Difference(EB->StmtGen[S], EB->Kill);
  for (auto Item : Diff)
    EB->Gen.insert(Item);

  // Add the Kill set of statement S_i to the Kill set of the block.
  for (const VarDecl *V : EB->StmtKill[S])
    EB->Kill.insert(V);
}

void
BoundsWideningAnalysis::FillStmtKillSetForModifiedVars(ElevatedCFGBlock *EB,
                                                       const Stmt *S) {
  // Get variables modified by statement S or statements nested in S.
  VarSetTy ModifiedVars;
  GetModifiedVars(S, ModifiedVars);

  // If a modified variable occurs in the bounds expression of a
  // null-terminated array then the bounds of that null-terminated array should
  // be killed.
  for (const VarDecl *ModifiedVar : ModifiedVars) {
    auto It = BoundsVarsLower.find(ModifiedVar);
    if (It != BoundsVarsLower.end()) {
      for (const VarDecl *V : It->second)
        EB->StmtKill[S].insert(V);
    }

    It = BoundsVarsUpper.find(ModifiedVar);
    if (It != BoundsVarsUpper.end()) {
      for (const VarDecl *V : It->second)
        EB->StmtKill[S].insert(V);
    }
  }
}

void
BoundsWideningAnalysis::FillStmtGenKillSetsForBoundsDecl(ElevatedCFGBlock *EB,
                                                         const Stmt *S,
                                                         const VarDecl *V) {
  if (IsNtArrayType(V) && V->hasBoundsExpr()) {
    EB->StmtGen[S][V] = SemaRef.NormalizeBounds(V);
    EB->StmtKill[S].insert(V);
  }
}

void
BoundsWideningAnalysis::FillStmtGenKillSetsForWhereClause(ElevatedCFGBlock *EB,
                                                          const Stmt *S,
                                                          WhereClause *WC) {
  if (!WC)
    return;

  for (const auto *Fact : WC->getFacts()) {
    auto *BF = dyn_cast<BoundsDeclFact>(Fact);
    if (!BF)
      continue;

    VarDecl *V = BF->Var;
    if (IsNtArrayType(V)) {
      EB->StmtGen[S][V] = SemaRef.ExpandBoundsToRange(V, BF->Bounds);
      EB->StmtKill[S].insert(V);
    }
  }
}

void
BoundsWideningAnalysis::FillStmtGenKillSetsForPtrDeref(ElevatedCFGBlock *EB,
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
      BoundsExpr *Bounds = SemaRef.NormalizeBounds(V);
      RangeBoundsExpr *RBE = dyn_cast<RangeBoundsExpr>(Bounds);
      assert(RBE && "invalid bounds for null-terminated array");

      // DerefExpr potentially widens V. So we add
      // "V:bounds(lower, DerefExpr + 1)" to the Gen set.
      R = new (Ctx) RangeBoundsExpr(RBE->getLowerExpr(),
                                    GetWidenedExpr(DerefExpr, 1),
                                    SourceLocation(), SourceLocation());
    }

    EB->StmtGen[S][V] = R;
    EB->StmtKill[S].insert(V);
  }
}

void BoundsWideningAnalysis::GetVarsToWiden(Expr *E, VarSetTy &VarsToWiden) {
  // Determine the set of variables that can be widened in an expression.
  if (!E)
    return;

  // Get all variables that occur in the expression.
  VarSetTy VarsInExpr;
  GetVarsInExpr(E, VarsInExpr);

  VarSetTy CommonNullTermArrays;
  for (const VarDecl *V : VarsInExpr) {
    // Get the set of null-terminated arrays in whose upper bounds expressions
    // V occurs.
    auto It = BoundsVarsUpper.find(V);

    // If V does not appear in the upper bounds expression of any
    // null-terminated array then expression E cannot potentially widen the
    // bounds of any null-terminated array.
    if (It == BoundsVarsUpper.end())
      return;

    if (CommonNullTermArrays.empty())
      CommonNullTermArrays = It->second;
    else
      CommonNullTermArrays = Intersect(CommonNullTermArrays, It->second);

    // If the intersection of CommonNullTermArrays is empty then expression E
    // cannot potentially widen the bounds of any null-terminated array.
    if (CommonNullTermArrays.empty())
      return;
  }

  for (const VarDecl *V : CommonNullTermArrays) {
    // Gather the set of variables occurring in the upper bound expression of
    // V.

    // TODO: This lookup can be optimized by storing the set of variables
    // occurring in the upper bounds expression of a null-terminated array
    // while computing BoundsVarsUpper in CheckedCAnalysesPrepass.cpp.
    VarSetTy VarsInBounds;
    for (auto Pair : BoundsVarsUpper) {
      if (Pair.second.count(V))
        VarsInBounds.insert(Pair.first);
    }

    if (IsEqual(VarsInExpr, VarsInBounds))
      VarsToWiden.insert(V);
  }
}

void BoundsWideningAnalysis::GetModifiedVars(const Stmt *S,
                                             VarSetTy &ModifiedVars) {
  // Get all variables modified by statement S or statements nested in S.
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

  E = IgnoreCasts(E);

  // Get variables in an expression like *(e1 + e2).
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

  for (auto Item : Ret) {
    if (!B.count(Item))
      Ret.erase(Item);
  }

  return Ret;
}

template<class T>
bool BoundsWideningAnalysis::IsEqual(T &A, T &B) const {
  return A.size() == B.size() &&
         A.size() == Intersect(A, B).size();
}

} // end namespace clang
