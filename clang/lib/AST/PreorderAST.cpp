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

void PreorderAST::AddNode(Node *N, Node *Parent) {
  // If the root is null, make the current node the root.
  if (!Root)
    Root = N;

  // Add the current node to the list of children of its parent.
  if (Parent) {
    assert(isa<BinaryNode>(Parent) && "Invalid parent");
    dyn_cast<BinaryNode>(Parent)->Children.push_back(N);
  }
}

void PreorderAST::CoalesceNode(BinaryNode *B, BinaryNode *Parent) {
  // Remove the current node from the list of children of its parent.
  for (auto I = Parent->Children.begin(),
            E = Parent->Children.end(); I != E; ++I) {
    if (*I == B) {
      Parent->Children.erase(I);
      break;
    }
  }

  // Move all children of the current node to its parent.
  for (auto *Child : B->Children) {
    Child->Parent = Parent;
    Parent->Children.push_back(Child);
  }

  // Delete the current node.
  delete B;
}

void PreorderAST::Create(Expr *E, Node *Parent) {
  if (!E)
    return;

  E = Lex.IgnoreValuePreservingOperations(Ctx, E->IgnoreParens());

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    auto *N = new BinaryNode(BO->getOpcode(), Parent);
    AddNode(N, Parent);

    Create(BO->getLHS(), /*Parent*/ N);
    Create(BO->getRHS(), /*Parent*/ N);

  } else {
    auto *N = new LeafExprNode(E, Parent);
    AddNode(N, Parent);
  }
}

void PreorderAST::Coalesce(Node *N, bool &Changed) {
  auto *B = dyn_cast_or_null<BinaryNode>(N);
  if (!B)
    return;

  // Coalesce the children first.
  for (auto *Child : B->Children)
    if (isa<BinaryNode>(Child))
      Coalesce(Child, Changed);

  // We can only coalesce if the operator is commutative and associative.
  if (!B->IsOpCommutativeAndAssociative())
    return;

  auto *Parent = dyn_cast_or_null<BinaryNode>(B->Parent);
  if (!Parent)
    return;

  // We can only coalesce if the parent has the same operator as the current
  // node.
  if (Parent->Opc != B->Opc)
    return;

  CoalesceNode(B, Parent);
  Changed = true;
}

void PreorderAST::Sort(Node *N) {
  auto *B = dyn_cast_or_null<BinaryNode>(N);
  if (!B)
    return;

  // Sort the children first.
  for (auto *Child : B->Children)
    if (isa<BinaryNode>(Child))
      Sort(Child);

  // We can only sort if the operator is commutative and associative.
  if (!B->IsOpCommutativeAndAssociative())
    return;

  // Sort the children.
  llvm::sort(B->Children.begin(), B->Children.end(),
  [&](const Node *N1, const Node *N2) {

    if (const auto *L1 = dyn_cast<LeafExprNode>(N1)) {
      if (const auto *L2 = dyn_cast<LeafExprNode>(N2))
        // If both nodes are LeafExprNodes compare the exprs.
        return Lex.CompareExpr(L1->E, L2->E) == Result::LessThan;
      // N2:BinaryNodeExpr < N1:LeafExprNode.
      return false;
    }

    // N1:BinaryNodeExpr < N2:LeafExprNode.
    if (isa<LeafExprNode>(N2))
      return true;

    // Compare N1:BinaryNode and N2:BinaryNode.
    const auto *B1 = dyn_cast<BinaryNode>(N1);
    const auto *B2 = dyn_cast<BinaryNode>(N2);

    if (B1->Opc != B2->Opc)
      return B1->Opc < B2->Opc;
    return B1->Children.size() < B2->Children.size();
  });
}

bool PreorderAST::IsEqual(Node *N1, Node *N2) {
  // If both the nodes are null.
  if (!N1 && !N2)
    return true;

  // If only one of the nodes is null.
  if ((N1 && !N2) || (!N1 && N2))
    return false;

  if (const auto *B1 = dyn_cast<BinaryNode>(N1)) {
    // If the types of the nodes mismatch.
    if (!isa<BinaryNode>(N2))
      return false;

    const auto *B2 = dyn_cast<BinaryNode>(N2);

    // If the Opcodes mismatch.
    if (B1->Opc != B2->Opc)
      return false;

    // If the number of children of the two nodes mismatch.
    if (B1->Children.size() != B2->Children.size())
      return false;

    // Match each child of the two nodes.
    for (size_t I = 0; I != B1->Children.size(); ++I) {
      auto *Child1 = B1->Children[I];
      auto *Child2 = B2->Children[I];
  
      // If any child differs between the two nodes.
      if (!IsEqual(Child1, Child2))
        return false;
    }
  }

  if (const auto *L1 = dyn_cast<LeafExprNode>(N1)) {
    // If the expr differs between the two nodes.
    if (const auto *L2 = dyn_cast<LeafExprNode>(N2))
      return Lex.CompareExpr(L1->E, L2->E) == Result::Equal;

    // Else if the types of the nodes mismatch.
    return false;
  }

  return true;
}

void PreorderAST::Normalize() {
  // TODO: Constant fold the constants in the nodes.
  // TODO: Perform simple arithmetic optimizations/transformations on the
  // constants in the nodes.

  bool Changed = true;
  while (Changed) {
    Changed = false;
    Coalesce(Root, Changed);
  }

  Sort(Root);
  PrettyPrint(Root);
}

void PreorderAST::PrettyPrint(Node *N) {
  if (const auto *B = dyn_cast_or_null<BinaryNode>(N)) {
    OS << BinaryOperator::getOpcodeStr(B->Opc) << "\n";

    for (auto *Child : B->Children)
      PrettyPrint(Child);
  }
  else if (const auto *L = dyn_cast_or_null<LeafExprNode>(N))
    L->E->dump(OS);
}

void PreorderAST::Cleanup(Node *N) {
  if (auto *B = dyn_cast_or_null<BinaryNode>(N))
    for (auto *Child : B->Children)
      Cleanup(Child);

  if (N)
    delete N;
}
