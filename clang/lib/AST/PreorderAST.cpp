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

void PreorderAST::Create(Expr *E, Node *N, Node *Parent) {
  if (!E)
    return;

  if (!N)
    N = new Node(Parent);

  // If the root is null, the current node is the root.
  if (!Root)
    Root = N;

  // If the parent is non-null and the current node has not already been added
  // to the list of children of the parent, add it.
  if (Parent) {
    if (!Parent->Children.size() || Parent->Children.back() != N)
      Parent->Children.push_back(N);
  }

  E = Lex.IgnoreValuePreservingOperations(Ctx, E);

  // If E is a variable, store it in the variable list for the current node.
  if (DeclRefExpr *D = GetDeclOperand(E)) {
    if (const auto *V = dyn_cast_or_null<VarDecl>(D->getDecl())) {
      N->Vars.push_back(V);
      return;
    }
  }

  // If E is a constant, store it in the constant field of the current node and
  // set the HasConst field.
  llvm::APSInt IntVal;
  if (E->isIntegerConstantExpr(IntVal, Ctx)) {
    N->Const = IntVal;
    N->HasConst = true;
    return;
  }

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    // Set the opcode for the current node.
    N->Opc = BO->getOpcode();

    Expr *LHS = BO->getLHS()->IgnoreParens();
    Expr *RHS = BO->getRHS()->IgnoreParens();

    if (isa<BinaryOperator>(LHS))
      // Create the LHS as a child of the current node.
      Create(LHS, nullptr, N);
    else
      // Create the LHS in the current node.
      Create(LHS, N);

    if (isa<BinaryOperator>(RHS))
      // Create the RHS as a child of the current node.
      Create(RHS, nullptr, N);
    else
      // Create the RHS in the current node.
      Create(RHS, N);

    return;
  }

  // Store all other non-variable, non-constant expressions here.
  N->Others.push_back(E);
}

bool PreorderAST::ConstantFold(Node *N, llvm::APSInt Val) {
  // Constant fold Val into the node N.

  if (!N) {
    SetError();
    return false;
  }

  // If N does not already have a constant simply make Val the constant
  // value for N.
  if (!N->HasConst) {
    N->Const = Val;
    N->HasConst = true;
    return true;
  }

  bool Overflow;
  switch (N->Opc) {
    default: return false;
    case BO_Add:
      N->Const = N->Const.sadd_ov(Val, Overflow);
      break;
    case BO_Mul:
      N->Const = N->Const.smul_ov(Val, Overflow);
      break;
  }

  if (Overflow)
    SetError();
  return !Overflow;
}

void PreorderAST::Coalesce(Node *N) {
  if (Error)
    return;

  if (!N)
    return;

  // Coalesce the children first.
  for (auto *Child : N->Children)
    Coalesce(Child);

  // For coalescing a node we would transfer its data to its parent. So if the
  // parent itself is null (for example, the root node) we cannot proceed.
  if (!N->Parent)
    return;

  Node *Parent = N->Parent;

  // We can only coalesce if the parent has the same opcode as the current
  // node.
  if (Parent->Opc != N->Opc)
    return;

  // Constant fold the constant of the current node with the constant of
  // the parent. Do not proceed if we could not fold the constant.
  if (N->HasConst) {
    if (!ConstantFold(Parent, N->Const))
      return;
  }

  // Move all the variables of the current node to the parent.
  Parent->Vars.insert(Parent->Vars.end(),
                      N->Vars.begin(),
                      N->Vars.end());

  // Move all other non-variable, non-constant expressions to the parent.
  Parent->Others.insert(Parent->Others.end(),
                        N->Others.begin(),
                        N->Others.end());

  // Remove the current node from the list of children of its parent.
  for (size_t I = 0; I != Parent->Children.size(); ++I) {
    if (Parent->Children[I] == N) {
      Parent->Children.erase(Parent->Children.begin() + I);
      break;
    }
  }

  // Move all children of the current node to the parent.
  Parent->Children.insert(Parent->Children.end(),
                          N->Children.begin(),
                          N->Children.end());

  // Delete the current node.
  delete N;
}

void PreorderAST::Sort(Node *N) {
  if (Error)
    return;

  if (!N)
    return;

  if (!N->IsOpCommutativeAndAssociative()) {
    SetError();
    return;
  }

  for (auto *Child : N->Children)
    Sort(Child);

  // Sort the variables in the node lexicographically.
  llvm::sort(N->Vars.begin(), N->Vars.end(),
             [&](const VarDecl *V1, const VarDecl *V2) {
               return Lex.CompareDecl(V1, V2) == Result::LessThan;
             });

  // Sort the other expressions in the node.
  llvm::sort(N->Others.begin(), N->Others.end(),
             [&](const Expr *E1, const Expr *E2) {
               return Lex.CompareExpr(E1, E2) == Result::LessThan;
             });

  // Sort the children nodes.
  llvm::sort(N->Children.begin(), N->Children.end(),
    [&](Node *N1, Node *N2) {
      // There is no single criteria for sorting the nodes. So we do our best
      // to sort the nodes.

      // Sort based on the values of the constants.
      if (N1->HasConst && N2->HasConst)
        return llvm::APSInt::compareValues(N1->Const, N2->Const) < 0;

      // Sort based on the number of variables.
      if (N1->Vars.size() != N2->Vars.size())
        return N1->Vars.size() < N2->Vars.size();

      // Sort based on the number of other expressions.
      if (N1->Others.size() != N2->Others.size())
        return N1->Others.size() < N2->Others.size();

      // Sort based on the number of children.
      if (N1->Children.size() != N2->Children.size())
        return N1->Children.size() < N2->Children.size();

      // Sort lexicographically on the variables of the two nodes.
      for (size_t I = 0; I != N1->Vars.size(); ++I) {
        auto &V1 = N1->Vars[I];
        auto &V2 = N2->Vars[I];

        if (Lex.CompareDecl(V1, V2) == Result::LessThan)
          return true;
      }

      // Sort based on the other expressions of the two nodes.
      for (size_t I = 0; I != N1->Others.size(); ++I) {
        auto &E1 = N1->Others[I];
        auto &E2 = N2->Others[I];

        if (Lex.CompareExpr(E1, E2) == Result::LessThan)
          return true;
      }

      return false;
    });
}

bool PreorderAST::IsEqual(Node *N1, Node *N2) {
  // If both the nodes are null.
  if (!N1 && !N2)
    return true;

  // If only one of the nodes is null.
  if ((N1 && !N2) || (!N1 && N2))
    return false;

  // If the Opcodes mismatch.
  if (N1->Opc != N2->Opc)
    return false;

  // If the number of variables in the two nodes mismatch.
  if (N1->Vars.size() != N2->Vars.size())
    return false;

  // If the values of the constants in the two nodes differ.
  if (llvm::APSInt::compareValues(N1->Const, N2->Const) != 0)
    return false;

  // If the number of other expressions in the two nodes mismatch.
  if (N1->Others.size() != N2->Others.size())
    return false;

  // If the number of children of the two nodes mismatch.
  if (N1->Children.size() != N2->Children.size())
    return false;

  // Match each variable occurring in the two nodes.
  for (size_t I = 0; I != N1->Vars.size(); ++I) {
    auto &V1 = N1->Vars[I];
    auto &V2 = N2->Vars[I];

    // If any variable differs between the two nodes.
    if (Lex.CompareDecl(V1, V2) != Result::Equal)
      return false;
  }

  // Match each of the other expressions occurring in the two nodes.
  for (size_t I = 0; I != N1->Others.size(); ++I) {
    auto &E1 = N1->Others[I];
    auto &E2 = N2->Others[I];

    // If any other expression differs between the two nodes.
    if (Lex.CompareExpr(E1, E2) != Result::Equal)
      return false;
  }

  // Match each child of the two nodes.
  for (size_t I = 0; I != N1->Children.size(); ++I) {
    auto *Child1 = N1->Children[I];
    auto *Child2 = N2->Children[I];

    // If any child differs between the two nodes.
    if (!IsEqual(Child1, Child2))
      return false;
  }
  return true;
}

void PreorderAST::Normalize() {
  // TODO: Perform simple arithmetic optimizations/transformations on the
  // constants in the nodes.

  Coalesce(Root);
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

void PreorderAST::PrettyPrint(Node *N) const {
  if (!N)
    return;

  OS << "Opc: { " << BinaryOperator::getOpcodeStr(N->Opc) << " }\n";

  if (N->Vars.size()) {
    OS << "Vars: { ";
    for (auto &V : N->Vars)
      OS << V->getQualifiedNameAsString() << " ";
    OS << "}\n";
  }

  if (N->HasConst)
    OS << "Const: { " << N->Const << " }\n";

  if (N->Others.size()) {
    OS << "Others: { ";
    for (auto &E : N->Others)
      E->dump(OS);
    OS << "}\n";
  }

  for (auto *Child : N->Children)
    PrettyPrint(Child);
}

void PreorderAST::Cleanup(Node *N) {
  if (!N)
    return;

  for (auto *Child : N->Children)
    Cleanup(Child);

  delete N;
}
