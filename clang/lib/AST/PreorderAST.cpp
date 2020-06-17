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
  if (auto *CE = dyn_cast<CastExpr>(E)) {
    assert(CE->getSubExpr() && "invalid CastExpr expression");

    if (CE->getCastKind() == CastKind::CK_LValueToRValue ||
        CE->getCastKind() == CastKind::CK_ArrayToPointerDecay)
      return isa<DeclRefExpr>(IgnoreCasts(CE->getSubExpr()));
  }
  return false;
}

DeclRefExpr *PreorderAST::GetDeclOperand(Expr *E) {
  if (!E || !isa<CastExpr>(E))
    return nullptr;
  auto *CE = dyn_cast<CastExpr>(E);
  assert(CE->getSubExpr() && "invalid CastExpr expression");

  return dyn_cast<DeclRefExpr>(IgnoreCasts(CE->getSubExpr()));
}

void PreorderAST::insert(ASTNode *N, Expr *E, ASTNode *Parent) {
  if (!E)
    return;

  // When we invoke insert(N->left, ...) or insert(N->right, ...) we need to
  // create the left or the right nodes with N as the parent node.
  if (!N)
    N = new ASTNode(Ctx, Parent);

  // If the parent is non-null, make sure that the current node is marked as a
  // child of the parent. As a convention, we create left children first.
  if (Parent) {
    if (!Parent->left)
      Parent->left = N;
    else
      Parent->right = N;
  }

  E = IgnoreCasts(E);

  // If E is a variable, store its name in the variable list for the current
  // node. Initialize the count of the variable to 1.
  if (IsDeclOperand(E)) {
    const DeclRefExpr *D = GetDeclOperand(E);
    if (const auto *V = dyn_cast_or_null<VarDecl>(D->getDecl())) {
      N->addVar(V->getQualifiedNameAsString());
      return;
    }
  }

  // If E is a constant, store it in the constant field of the current node and
  // mark that this node has a constant.
  llvm::APSInt IntVal;
  if (E->isIntegerConstantExpr(IntVal, Ctx)) {
    N->constant = IntVal;
    N->HasConstant = true;
    return;
  }

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    OpcodeTy Opc = BO->getOpcode();
    Expr *LHS = BO->getLHS()->IgnoreParens();
    Expr *RHS = BO->getRHS()->IgnoreParens();

    // Set the opcode for the current node.
    N->opcode = Opc;

    if (isa<BinaryOperator>(LHS))
      // Insert the LHS as the left child of the current node.
      insert(N->left, LHS, /* parent node */ N);
    else
      // Insert the LHS in the current node.
      insert(N, LHS);

    if (isa<BinaryOperator>(RHS))
      // Insert the RHS as the right child of the current node.
      insert(N->right, RHS, /* parent node */ N);
    else
      // Insert the RHS in the current node.
      insert(N, RHS);
  }
}

void PreorderAST::sort(ASTNode *N, bool &HasError) {
  if (HasError)
    return;

  if (!N || !N->variables.size())
    return;

  if (!N->isOpCommutativeAndAssociative()) {
    HasError = true;
    return;
  }

  // Sort the variables in the node lexicographically.
  llvm::sort(N->variables.begin(), N->variables.end(),
             [](VarTy a, VarTy b) {
               return a.name.compare(b.name) < 0;
             });

  sort(N->left, HasError);
  sort(N->right, HasError);
}

void PreorderAST::normalize(ASTNode *N, bool &HasError) {
  sort(N, HasError);
}

bool PreorderAST::isEqual(ASTNode *N1, ASTNode *N2) {
  // If both the nodes are null.
  if (!N1 && !N2)
    return true;

  // If only one of the nodes is null.
  if ((N1 && !N2) || (!N1 && N2))
    return false;

  // If the opcodes mismatch.
  if (N1->opcode != N2->opcode)
    return false;

  // If the number of variables in the two nodes mismatch.
  if (N1->variables.size() != N2->variables.size())
    return false;

  // If the values of the constants in the two nodes differ.
  if (llvm::APSInt::compareValues(N1->constant, N2->constant) != 0)
    return false;

  // Match each variable occurring in the two nodes.
  for (size_t i = 0; i != N1->variables.size(); ++i) {
    auto &V1 = N1->variables[i];
    auto &V2 = N2->variables[i];

    // If any variable differs between the two nodes.
    if (V1.name.compare(V2.name) != 0)
      return false;

    // If the count of any variable differs.
    if (V1.count != V2.count)
      return false;
  }

  // Recursively match the left and the right subtrees of the AST.
  return isEqual(N1->left, N2->left) &&
         isEqual(N1->right, N2->right);
}

Result PreorderAST::compare(PreorderAST &PT) {
  bool areExprsEqual = isEqual(AST, PT.AST);
  // Cleanup memory consumed by the ASTs.
  cleanup();
  PT.cleanup();

  if (areExprsEqual)
    return Result::Equal;
  return Result::NotEqual;
}

void PreorderAST::print(ASTNode *N) {
  if (!N)
    return;

  OS << BinaryOperator::getOpcodeStr(N->opcode);
  if (N->variables.size()) {
    for (auto &V : N->variables)
      OS << " [" << V.name << ":" << V.count << "]";
  }

  if (N->HasConstant)
    OS << " [const:" << N->constant << "]";
  OS << "\n";

  print(N->left);
  print(N->right);
}

void PreorderAST::cleanup(ASTNode *N) {
  if (!N)
    return;

  cleanup(N->left);
  cleanup(N->right);

  delete N;
}

void PreorderAST::cleanup() {
  cleanup(AST);
}
