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
  return E && isa<DeclRefExpr>(E);
}

DeclRefExpr *PreorderAST::GetDeclOperand(Expr *E) {
  if (!E)
    return nullptr;

  if (isa<DeclRefExpr>(E))
    return dyn_cast<DeclRefExpr>(E);

  if (!isa<CastExpr>(E))
    return nullptr;

  auto *CE = dyn_cast<CastExpr>(E);
  assert(CE->getSubExpr() && "invalid CastExpr expression");

  return dyn_cast<DeclRefExpr>(IgnoreCasts(CE->getSubExpr()));
}

void PreorderAST::insert(ASTNode *N, Expr *E, ASTNode *Parent) {
  if (!E)
    return;

  if (!N)
    N = new ASTNode(Ctx, Parent);

  if (Parent) {
    if (!Parent->left)
      Parent->left = N;
    else
      Parent->right = N;
  }

  E = IgnoreCasts(E);

  if (IsDeclOperand(E)) {
    const DeclRefExpr *D = GetDeclOperand(E);
    if (const auto *V = dyn_cast_or_null<VarDecl>(D->getDecl())) {
      N->addVar(V->getQualifiedNameAsString());
      return;
    }
  }

  llvm::APSInt IntVal;
  if (E->isIntegerConstantExpr(IntVal, Ctx)) {
    N->constant = IntVal;
    N->hasConstant = true;
    return;
  }

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    OpcodeTy Opc = BO->getOpcode();
    Expr *LHS = BO->getLHS()->IgnoreParens();
    Expr *RHS = BO->getRHS()->IgnoreParens();

    N->opcode = Opc;

    if (isa<BinaryOperator>(LHS))
      insert(N->left, LHS, N);
    else
      insert(N, LHS);

    if (isa<BinaryOperator>(RHS))
      insert(N->right, RHS, N);
    else
      insert(N, RHS);
  }
}

void PreorderAST::coalesceConst(ASTNode *N, llvm::APSInt IntVal) {
  if (!N->hasConstant) {
    N->constant = IntVal;
    return;
  }

  bool Overflow;
  switch(N->opcode) {
    default: return;
    case BO_Add:
      N->constant = N->constant.sadd_ov(IntVal, Overflow);
      break;
    case BO_Mul:
      N->constant = N->constant.smul_ov(IntVal, Overflow);
      break;
  }
  assert(!Overflow);
}

void PreorderAST::coalesce(ASTNode *N) {
  if (!N)
    return;

  if (!N->isLeafNode()) {
    coalesce(N->left);
    coalesce(N->right);
  }

  ASTNode *Parent = N->parent;
  if (Parent && Parent->opcode == N->opcode) {
    if (N->hasConstant)
      coalesceConst(Parent, N->constant);

    for (auto &V : N->variables)
      Parent->addVar(V);
  
    if (N == Parent->left)
      Parent->left = nullptr;
    else
      Parent->right = nullptr;

    delete N;
  }
}

void PreorderAST::sort(ASTNode *N) {
  if (!N || !N->variables.size())
    return;

  llvm::sort(N->variables.begin(), N->variables.end(),
             [](VarTy a, VarTy b) {
               return a.compare(b) < 0;
             });

  sort(N->left);
  sort(N->right);
}

Result PreorderAST::compare(ASTNode *N1, ASTNode *N2) {
  if (!N1 && !N2)
    return Result::Equal;

  if ((N1 && !N2) || (!N1 && N2))
    return Result::NotEqual;

  if (N1->opcode != N2->opcode)
    return Result::NotEqual;

  if (N1->variables.size() != N2->variables.size())
    return Result::NotEqual;

  if (N1->constant != N2->constant)
    return Result::NotEqual;

  for (size_t i = 0; i != N1->variables.size(); ++i) {
    auto &V1 = N1->variables[i];
    auto &V2 = N2->variables[i];
    if (V1.compare(V2) != 0)
      return Result::NotEqual;
  }

  Result LHSResult = compare(N1->left, N2->left);
  if (LHSResult == Result::NotEqual)
    return Result::NotEqual;

  Result RHSResult = compare(N1->right, N2->right);
  if (RHSResult == Result::NotEqual)
    return Result::NotEqual;

  return Result::Equal;
}

void PreorderAST::normalize(ASTNode *N) {
}

Result PreorderAST::compare(PreorderAST &PT) {
  return compare(AST, PT.AST);
}

void PreorderAST::print(ASTNode *N) {
  if (!N)
    return;

  OS << BinaryOperator::getOpcodeStr(N->opcode);
  if (N->variables.size()) {
    for (auto &V : N->variables)
      OS << " " << V;
  }
  OS << " " << N->constant << "\n";

  print(N->left);
  print(N->right);
}
