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

bool PreorderAST::IsDeclOperand(Expr *E, DeclRefExpr *&D) {
  if (auto *CE = dyn_cast_or_null<CastExpr>(E)) {
    assert(CE->getSubExpr() && "invalid CastExpr expression");

    if (CE->getCastKind() == CastKind::CK_LValueToRValue ||
        CE->getCastKind() == CastKind::CK_ArrayToPointerDecay) {
      E = Lex.IgnoreValuePreservingOperations(Ctx, CE->getSubExpr());
      if (auto *DRE = dyn_cast_or_null<DeclRefExpr>(E)) {
        D = DRE;
        return true;
      }
    }
  }
  return false;
}

void PreorderAST::Create(ASTNode *N, Expr *E, ASTNode *Parent) {
  if (!E)
    return;

  // When we invoke Create(N->Left, ...) or Create(N->Right, ...) we need to
  // create the Left or the Right nodes with N as the Parent node.
  if (!N)
    N = new ASTNode(Ctx, Parent);

  // If the Parent is non-null, make sure that the current node is marked as a
  // child of the Parent. As a convention, we create Left children first.
  if (Parent) {
    if (!Parent->Left)
      Parent->Left = N;
    else
      Parent->Right = N;
  }

  E = Lex.IgnoreValuePreservingOperations(Ctx, E);

  // If E is a variable, store its Name in the variable list for the current
  // node. Initialize the count of the variable to 1.
  DeclRefExpr *D;
  if (IsDeclOperand(E, D)) {
    if (const auto *V = dyn_cast_or_null<VarDecl>(D->getDecl())) {
      N->AddVar(V->getQualifiedNameAsString());
      return;
    }
  }

  // If E is a Constant, store it in the Constant field of the current node and
  // mark that this node has a Constant.
  llvm::APSInt IntVal;
  if (E->isIntegerConstantExpr(IntVal, Ctx)) {
    N->Constant = IntVal;
    N->HasConstant = true;
    return;
  }

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    OpcodeTy Opc = BO->getOpcode();
    Expr *LHS = BO->getLHS()->IgnoreParens();
    Expr *RHS = BO->getRHS()->IgnoreParens();

    // Set the Opcode for the current node.
    N->Opcode = Opc;

    if (isa<BinaryOperator>(LHS))
      // Create the LHS as the Left child of the current node.
      Create(N->Left, LHS, /* Parent node */ N);
    else
      // Create the LHS in the current node.
      Create(N, LHS);

    if (isa<BinaryOperator>(RHS))
      // Create the RHS as the Right child of the current node.
      Create(N->Right, RHS, /* Parent node */ N);
    else
      // Create the RHS in the current node.
      Create(N, RHS);
  }
}

void PreorderAST::Sort(ASTNode *N, bool &Error) {
  if (Error)
    return;

  if (!N || !N->Variables.size())
    return;

  if (!N->IsOpCommutativeAndAssociative()) {
    Error = true;
    return;
  }

  // Sort the variables in the node lexicographically.
  llvm::sort(N->Variables.begin(), N->Variables.end(),
             [](VarTy a, VarTy b) {
               return a.Name.compare(b.Name) < 0;
             });

  Sort(N->Left, Error);
  Sort(N->Right, Error);
}

void PreorderAST::Normalize(ASTNode *N, bool &Error) {
  Sort(N, Error);
}

bool PreorderAST::IsEqual(ASTNode *N1, ASTNode *N2) {
  // If both the nodes are null.
  if (!N1 && !N2)
    return true;

  // If only one of the nodes is null.
  if ((N1 && !N2) || (!N1 && N2))
    return false;

  // If the Opcodes mismatch.
  if (N1->Opcode != N2->Opcode)
    return false;

  // If the number of variables in the two nodes mismatch.
  if (N1->Variables.size() != N2->Variables.size())
    return false;

  // If the values of the Constants in the two nodes differ.
  if (llvm::APSInt::compareValues(N1->Constant, N2->Constant) != 0)
    return false;

  // Match each variable occurring in the two nodes.
  for (size_t i = 0; i != N1->Variables.size(); ++i) {
    auto &V1 = N1->Variables[i];
    auto &V2 = N2->Variables[i];

    // If any variable differs between the two nodes.
    if (V1.Name.compare(V2.Name) != 0)
      return false;

    // If the count of any variable differs.
    if (V1.Count != V2.Count)
      return false;
  }

  // Recursively match the Left and the Right subtrees of the AST.
  return IsEqual(N1->Left, N2->Left) &&
         IsEqual(N1->Right, N2->Right);
}

Result PreorderAST::Compare(PreorderAST &PT) {
  if (IsEqual(AST, PT.AST))
    return Result::Equal;
  return Result::NotEqual;
}

void PreorderAST::PrettyPrint(ASTNode *N) {
  if (!N)
    return;

  OS << BinaryOperator::getOpcodeStr(N->Opcode);
  if (N->Variables.size()) {
    for (auto &V : N->Variables)
      OS << " [" << V.Name << ":" << V.Count << "]";
  }

  if (N->HasConstant)
    OS << " [const:" << N->Constant << "]";
  OS << "\n";

  PrettyPrint(N->Left);
  PrettyPrint(N->Right);
}

void PreorderAST::Cleanup(ASTNode *N) {
  if (!N)
    return;

  Cleanup(N->Left);
  Cleanup(N->Right);

  delete N;
}

void PreorderAST::Cleanup() {
  Cleanup(AST);
}
