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

void PreorderAST::Create(Expr *E, Node *Parent) {
  if (!E)
    return;

  E = Lex.IgnoreValuePreservingOperations(Ctx, E->IgnoreParens());

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    auto *N = new BinaryNode(Parent, BO->getOpcode());
    if (!Root)
      Root = N;

    if (Parent)
      Parent->Children.push_back(N);

    Create(BO->getLHS(), /*Parent*/ N);
    Create(BO->getRHS(), /*Parent*/ N);

  } else {
    LeafExprNode *N = nullptr;

    if (Parent) {
      for (auto *Child : Parent->Children) {
        if (auto *L = dyn_cast<LeafExprNode>(Child)) {
          N = L;
          break;
        }
      }
    }

    if (!N) {
      N = new LeafExprNode(Parent);
      if (!Root)
        Root = N;
    
      if (Parent)
        Parent->Children.push_back(N);
    }
        
    N->Exp.push_back(E);
  }
}

void PreorderAST::Sort(Node *N) {
  if (Error)
    return;

  if (!N)
    return;

  if (auto *B = dyn_cast<BinaryNode>(N)) {
    if (!B->IsOpCommutativeAndAssociative()) {
      SetError();
      return;
    }

    for (auto *Child : N->Children)
      Sort(Child);
  }

  if (auto *L = dyn_cast<LeafExprNode>(N)) {
    // Sort the exprs in the node lexicographically.
    llvm::sort(L->Exp.begin(), L->Exp.end(),
               [&](const Expr *E1, const Expr *E2) {
                 return Lex.CompareExpr(E1, E2) == Result::LessThan;
               });
  }
}

bool PreorderAST::IsEqual(Node *N1, Node *N2) {
  // If both the nodes are null.
  if (!N1 && !N2)
    return true;

  // If only one of the nodes is null.
  if ((N1 && !N2) || (!N1 && N2))
    return false;

  if (auto *B1 = dyn_cast<BinaryNode>(N1)) {
    // If the types of the nodes mismatch.
    if (!isa<BinaryNode>(N2))
      return false;

    auto *B2 = dyn_cast<BinaryNode>(N2);

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

  if (auto *L1 = dyn_cast<LeafExprNode>(N1)) {
    // If the types of the nodes mismatch.
    if (!isa<LeafExprNode>(N2))
      return false;

    auto *L2 = dyn_cast<LeafExprNode>(N2);

    // If the number of exprs in the two nodes mismatch.
    if (L1->Exp.size() != L2->Exp.size())
      return false;

    // Match each expr occurring in the two nodes.
    for (size_t I = 0; I != L1->Exp.size(); ++I) {
      auto &E1 = L1->Exp[I];
      auto &E2 = L2->Exp[I];
  
      // If any expr differs between the two nodes.
      if (Lex.CompareExpr(E1, E2) != Result::Equal)
        return false;
    }
  }

  return true;
}

void PreorderAST::Normalize() {
  // TODO: Coalesce nodes having the same commutative and associative operator.
  // TODO: Constant fold the constants in the nodes.
  // TODO: Perform simple arithmetic optimizations/transformations on the
  // constants in the nodes.

  Sort(Root);
}

DeclRefExpr *PreorderAST::GetDeclOperand(Expr *E) {
  if (auto *CE = dyn_cast_or_null<CastExpr>(E)) {
    assert(CE->getSubExpr() && "Invalid CastExpr expression");

    if (CE->getCastKind() == CastKind::CK_LValueToRValue ||
        CE->getCastKind() == CastKind::CK_ArrayToPointerDecay) {
      E = Lex.IgnoreValuePreservingOperations(Ctx, CE->getSubExpr());
      return dyn_cast_or_null<DeclRefExpr>(E);
    }
  }
  return nullptr;
}

void PreorderAST::PrettyPrint(Node *N) {
  if (!N)
    return;

  if (const auto *B = dyn_cast<BinaryNode>(N)) {
    OS << BinaryOperator::getOpcodeStr(B->Opc) << "\n";

    for (auto *Child : B->Children)
      PrettyPrint(Child);
  }
  else if (const auto *L = dyn_cast<LeafExprNode>(N)) {
    for (auto &E : L->Exp)
      E->dump(OS);
  }
}

void PreorderAST::Cleanup(Node *N) {
  if (!N)
    return;

  if (const auto *B = dyn_cast<BinaryNode>(N)) {
    for (auto *Child : B->Children)
      Cleanup(Child);
  }

  delete N;
}
