//===------ PreorderAST.cpp: An n-ary preorder abstract syntax tree -------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements methods to create and manipulate an n-ary preorder
//  abstract syntax tree which is used to semantically compare two expressions.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/PreorderAST.h"

using namespace clang;

Expr *PreorderAST::IgnoreCasts(const Expr *E) {
  return Lex.IgnoreValuePreservingOperations(Ctx, const_cast<Expr *>(E));
}

bool PreorderAST::IsDeclOperand(Expr *E) {
  if (auto *CE = dyn_cast_or_null<CastExpr>(E)) {
    assert(CE->getSubExpr() && "invalid CastExpr expression");

    if (CE->getCastKind() == CastKind::CK_LValueToRValue ||
        CE->getCastKind() == CastKind::CK_ArrayToPointerDecay)
      return isa<DeclRefExpr>(IgnoreCasts(CE->getSubExpr()));
  }
  return false;
}

void PreorderAST::insert(Expr *E, ASTNode *CurrNode, ASTNode *Parent) {
  if (!E)
    return;

  E = IgnoreCasts(E);

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    Expr *LHS = BO->getLHS()->IgnoreParens();
    Expr *RHS = BO->getRHS()->IgnoreParens();

    CurrNode->data = new ASTData(BO->getOpcode());

    if (IsDeclOperand(LHS))
      CurrNode->data->addOperand(LHS);
    else {
      CurrNode->left = new ASTNode(CurrNode);
      insert(LHS, CurrNode->left);
    }

    if (IsDeclOperand(RHS))
      CurrNode->data->addOperand(RHS);
    else {
      CurrNode->right = new ASTNode(CurrNode);
      insert(RHS, CurrNode->right);
    }
  }
}

void PreorderAST::print(ASTNode *N) {
  if (!N || !N->data || !N->data->getOperandSize())
    return;

  OS << "Operator: " << N->data->getOpcode() << "\n";
  print(N->left);
  print(N->right);
}
