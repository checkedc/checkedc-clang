//===--------- NormalizeUtils.cpp: Functions for normalizing expressions --===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements functions for normalizing expressions.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ExprUtils.h"
#include "clang/AST/NormalizeUtils.h"

using namespace clang;

// Input form:  E1 - E2
// Output form: E1 + -E2
//
// This transformation is applied to E1 and E2 as well.
Expr *NormalizeUtil::TransformAdditiveOp(Sema &S, Expr *E) {
  // Check that E is of the form E1 +/- E2.
  BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens());
  if (!BO)
    return nullptr;
  if (!BinaryOperator::isAdditiveOp(BO->getOpcode()))
    return nullptr;

  // If E1 is of the form E3 - E4, transform E1 to E3 + -E4.
  Expr *E1 = TransformSingleAdditiveOp(S, BO->getLHS());

  // If E2 is of the form E5 - E6, transform E2 to E5 + -E6.
  Expr *E2 = TransformSingleAdditiveOp(S, BO->getRHS());

  // Negate E2 if E is of the form E1 - E2.
  if (BO->getOpcode() == BinaryOperatorKind::BO_Sub)
    E2 = ExprCreatorUtil::CreateUnaryOperator(S, E2,
                             UnaryOperatorKind::UO_Minus);
  return AddExprs(S, E1, E2);
}

// Input form:  E1 - E2
// Output form: E1 + -E2
Expr *NormalizeUtil::TransformSingleAdditiveOp(Sema &S, Expr *E) {
  BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens());
  if (!BO)
    return E;
  if (BO->getOpcode() != BinaryOperatorKind::BO_Sub)
    return E;

  Expr *LHS = BO->getLHS();
  Expr *RHS = BO->getRHS();
  Expr *Minus = ExprCreatorUtil::CreateUnaryOperator(S, RHS,
                                   UnaryOperatorKind::UO_Minus);
  return AddExprs(S, LHS, Minus);
}

Expr *NormalizeUtil::AddExprs(Sema &S, Expr *LHS, Expr *RHS) {
  return ExprCreatorUtil::CreateBinaryOperator(S, LHS, RHS,
                            BinaryOperatorKind::BO_Add);
}

bool NormalizeUtil::GetAdditionOperands(Expr *E, Expr *&LHS, Expr *&RHS) {
  BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens());
  if (!BO)
    return false;
  if (BO->getOpcode() != BinaryOperatorKind::BO_Add)
    return false;
  LHS = BO->getLHS();
  RHS = BO->getRHS();
  return true;
}

bool NormalizeUtil::GetRHSConstant(Sema &S, BinaryOperator *E, QualType T,
                                   llvm::APSInt &Constant) {
  if (!E->isAdditiveOp())
    return false;
  if (!E->getRHS()->isIntegerConstantExpr(Constant, S.Context))
    return false;

  bool Overflow;
  Constant = ExprUtil::ConvertToSignedPointerWidth(S.Context, Constant, Overflow);
  if (Overflow)
    return false;
  // Normalize the operation by negating the offset if necessary.
  if (E->getOpcode() == BO_Sub) {
    uint64_t PointerWidth = S.Context.getTargetInfo().getPointerWidth(0);
    Constant = llvm::APSInt(PointerWidth, false).ssub_ov(Constant, Overflow);
    if (Overflow)
      return false;
  }
  llvm::APSInt ElemSize;
  if (!ExprUtil::getReferentSizeInChars(S.Context, T, ElemSize))
    return false;
  Constant = Constant.smul_ov(ElemSize, Overflow);
  if (Overflow)
    return false;

  return true;
}
