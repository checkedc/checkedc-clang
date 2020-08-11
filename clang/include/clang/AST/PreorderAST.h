//===------- PreorderAST.h: An n-ary preorder abstract syntax tree -------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the interface for an n-ary preorder abstract syntax tree
//  which is used to semantically compare two expressions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_PREORDER_AST_H
#define LLVM_CLANG_PREORDER_AST_H

#include "clang/AST/ASTContext.h"
#include "clang/AST/CanonBounds.h"
#include "clang/AST/Expr.h"

namespace clang {

  using Result = Lexicographic::Result;

  // Each binary operator of an expression results in a new node of the
  // PreorderAST. Each node contains the following fields:

  // Opc: The opcode of the operator.
  // Vars: A list of variables in the sub expression.
  // Const: Constants of the sub expression are folded.
  // HasConst: Indicates whether there is a constant in the node. It is used to
  // differentiate between the absence of a constant and a constant value of 0.
  // Parent: A link to the parent node of the current node.
  // Children: The preorder AST is an n-ary tree. Children is a list of all the
  // child nodes of the current node.

  struct Node {
    BinaryOperator::Opcode Opc;
    std::vector<const VarDecl *> Vars;
    llvm::APSInt Const;
    bool HasConst;
    Node *Parent;
    llvm::SmallVector<Node *, 2> Children;

    Node(Node *Parent) :
      Opc(BO_Add), HasConst(false), Parent(Parent) {}

    // Is the operator commutative and associative?
    bool IsOpCommutativeAndAssociative() {
      return Opc == BO_Add || Opc == BO_Mul;
    }
  };

  class PreorderAST {
  private:
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;
    bool Error;
    Node *Root;

    // Create a PreorderAST for the expression E.
    // @param[in] E is the sub expression which needs to be added to N.
    // @param[in] N is the current node of the AST.
    // @param[in] Parent is the parent node for N.
    void Create(Expr *E, Node *N = nullptr, Node *Parent = nullptr);

    // Sort the variables in a node of the AST.
    // @param[in] N is current node of the AST.
    void Sort(Node *N);

    // Check if the two AST nodes N1 and N2 are equal.
    // @param[in] N1 is the first node.
    // @param[in] N2 is the second node.
    // @return Returns a boolean indicating whether N1 and N2 are equal.
    bool IsEqual(Node *N1, Node *N2);

    // Set Error in case an error occurs during transformation of the AST.
    void SetError() { Error = true; }

    // Print the PreorderAST.
    // @param[in] N is the current node of the AST.
    void PrettyPrint(Node *N);

    // Cleanup the memory consumed by node N.
    // @param[in] N is the current node of the AST.
    void Cleanup(Node *N);

    // A DeclRefExpr can be a reference either to an array subscript (in which
    // case it is wrapped around a ArrayToPointerDecay cast) or to a pointer
    // dereference (in which case it is wrapped around an LValueToRValue cast).
    // @param[in] An expression E.
    // @return Returns a DeclRefExpr if E is a DeclRefExpr, otherwise nullptr.
    DeclRefExpr *GetDeclOperand(Expr *E);

  public:
    PreorderAST(ASTContext &Ctx, Expr *E) :
      Ctx(Ctx), Lex(Lexicographic(Ctx, nullptr)), OS(llvm::outs()),
      Error(false), Root(nullptr) {
      Create(E);
    }

    // Normalize the input expression through a series of transforms on the
    // preorder AST. The Error field is set if an error is encountered during
    // transformation of the AST.
    void Normalize();

    // Check if the two ASTs are equal. This is intended to be called from
    // outside this class and invokes IsEqual on the root nodes of the two ASTs
    // to recursively compare the AST nodes.
    // @param[in] this is the first AST.
    // @param[in] P is the second AST.
    // @return Returns a bool indicating whether the two ASTs are equal.
    bool IsEqual(PreorderAST &P) { return IsEqual(Root, P.Root); }

    // Check if an error has occurred during transformation of the AST. This
    // is intended to be called from outside this class to check if an error
    // has occurred during comparison of expressions.
    // @return Whether an error has occurred or not.
    bool GetError() { return Error; }

    // Cleanup the memory consumed by the AST. This is intended to be called
    // from outside this class and invokes Cleanup on the root node which
    // recursively deletes the AST.
    void Cleanup() { Cleanup(Root); }
  };

} // end namespace clang
#endif
