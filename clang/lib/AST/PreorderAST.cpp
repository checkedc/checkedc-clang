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

  // If the parent is non-null, make sure that the current node is marked as a
  // child of the parent. As a convention, we create left children first.
  if (Parent) {
    if (!Parent->Left)
      Parent->Left = N;
    else
      Parent->Right = N;
  }

  E = Lex.IgnoreValuePreservingOperations(Ctx, E);

  // If E is a variable, store its name in the variable list for the current
  // node.
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
      // Create the LHS as the left child of the current node.
      Create(LHS, N->Left, N);
    else
      // Create the LHS in the current node.
      Create(LHS, N);
  
    if (isa<BinaryOperator>(RHS))
      // Create the RHS as the right child of the current node.
      Create(RHS, N->Right, N);
    else
      // Create the RHS in the current node.
      Create(RHS, N);
  
    return;
  }

  // Currently, we only handle expression which are either variables or
  // constants.
  // TODO: Handle expressions that are non-variables and non-constants.
  // Possibly, add a field to the node to represent such expressions.
  SetError();
}

bool PreorderAST::ConstantFold(Node *N, llvm::APSInt ConstVal) {
  // Constant fold ConstVal into the node N.

  // If N does not already have a constant simply make ConstVal the constant
  // value for N.
  if (!N->HasConst) {
    N->Const = ConstVal;
    N->HasConst = true;
    return true;
  }

  bool Overflow;
  switch (N->Opc) {
    default: return false;
    case BO_Add:
      N->Const = N->Const.sadd_ov(ConstVal, Overflow);
       break;
    case BO_Mul:
      N->Const = N->Const.smul_ov(ConstVal, Overflow);
       break;
  }

  if (Overflow)
    SetError();
  return !Overflow;
}

void PreorderAST::SwapChildren(Node *N) {
  if (!N)
    return;

  Node *Tmp = N->Left;
  N->Left = N->Right;
  N->Right = Tmp;
}

void PreorderAST::Coalesce(Node *N) {
  if (Error)
    return;

  if (!N)
    return;

  // Coalesce the leaf nodes first.
  if (!N->IsLeafNode()) {
    Coalesce(N->Left);
    Coalesce(N->Right);
  }

  // For coalescing a node we would transfer its variables and constant to its
  // parent. So if the parent itself is null (for example, the root node) we
  // cannot proceed.
  if (!N->Parent)
    return;

  // Currently, we only coalese leaf nodes into non-leaf nodes.
  // TODO: Coalesce non-leaf nodes?
  if (!N->IsLeafNode())
    return;

  Node *Parent = N->Parent;

  // We can only coalesce if the parent has the same opcode as the current
  // node.
  if (Parent->Opc != N->Opc)
    return;

  // Move all the variables of the current node to the parent.
  for (auto &V : N->Vars)
    Parent->Vars.push_back(V);

  // Constant fold the constant of the current node with the constant of
  // the parent. Do not proceed if we could not fold the constant.
  if (N->HasConst) {
    if (!ConstantFold(Parent, N->Const))
      return;
  }

  // Remove current node from its parent.
  if (N == Parent->Left)
    Parent->Left = nullptr;
  else
    Parent->Right = nullptr;

  // Cleanup the current node.
  Cleanup(N);
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

  Sort(N->Left);
  Sort(N->Right);

  if (N->Vars.size() > 1) {
    // Sort the variables in the node lexicographically.
    llvm::sort(N->Vars.begin(), N->Vars.end(),
               [](const VarDecl *V1, const VarDecl *V2) {
                 return V1->getQualifiedNameAsString().compare(
                        V2->getQualifiedNameAsString()) < 0;
               });
  }

  // Now, sort the children nodes of the current node.
  // We need to sort the nodes only if the current node has both children
  // nodes.
  if (!N->Left || !N->Right)
    return;

  size_t NumLHSVars = N->Left->Vars.size();
  size_t NumRHSVars = N->Right->Vars.size();

  // Make the node with the lesser number of variables the left child of the
  // current node.
  if (NumRHSVars < NumLHSVars) {
    SwapChildren(N);
    return;
  }

  // If both the children have the same number of variables, compare the
  // variables lexicographically to sort the nodes.
  if (NumLHSVars == NumRHSVars) {
    for (size_t I = 0; I != NumLHSVars; ++I) {
      auto &V1 = N->Left->Vars[I];
      auto &V2 = N->Right->Vars[I];

      if (V1->getQualifiedNameAsString().compare(
          V2->getQualifiedNameAsString()) > 0) {
        SwapChildren(N);
        return;
      }
    }
  }
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

  // Match each variable occurring in the two nodes.
  for (size_t I = 0; I != N1->Vars.size(); ++I) {
    auto &V1 = N1->Vars[I];
    auto &V2 = N2->Vars[I];

    // If any variable differs between the two nodes.
    if (V1->getQualifiedNameAsString().compare(
        V2->getQualifiedNameAsString()) != 0)
      return false;
  }

  // Recursively match the left and the right subtrees of the AST.
  return IsEqual(N1->Left, N2->Left) &&
         IsEqual(N1->Right, N2->Right);
}

void PreorderAST::Normalize() {
  // TODO: Perform simple arithmetic optimizations/transformations on the
  // constants in the nodes.

  Coalesce(Root);
  Sort(Root);
  PrettyPrint(Root);
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

  OS << BinaryOperator::getOpcodeStr(N->Opc);

  for (auto &V : N->Vars)
    OS << " [" << V->getQualifiedNameAsString() << "]";

  if (N->HasConst)
    OS << " [const:" << N->Const << "]";

  OS << "\n";

  PrettyPrint(N->Left);
  PrettyPrint(N->Right);
}

void PreorderAST::Cleanup(Node *N) {
  if (!N)
    return;

  Cleanup(N->Left);
  Cleanup(N->Right);

  delete N;
}
