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
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/CanonBounds.h"
#include "clang/AST/Expr.h"

namespace clang {
  using Result = Lexicographic::Result;

  class Node {
  public:
    enum class NodeKind { BinaryNode, LeafExprNode };

    NodeKind Kind;
    Node *Parent;

    Node(NodeKind Kind, Node *Parent) :
      Kind(Kind), Parent(Parent) {}
  };

  class BinaryNode : public Node {
  public:
    BinaryOperator::Opcode Opc;
    llvm::SmallVector<Node *, 2> Children;

    BinaryNode(BinaryOperator::Opcode Opc, Node *Parent) :
      Node(NodeKind::BinaryNode, Parent),
      Opc(Opc) {}

    static bool classof(const Node *N) {
      return N->Kind == NodeKind::BinaryNode;
    }

    // Is the operator commutative and associative?
    bool IsOpCommutativeAndAssociative() {
      return Opc == BO_Add || Opc == BO_Mul;
    }
  };

  class LeafExprNode : public Node {
  public:
    Expr *E;

    LeafExprNode(Expr *E, Node *Parent) :
      Node(NodeKind::LeafExprNode, Parent),
      E(E) {}

    static bool classof(const Node *N) {
      return N->Kind == NodeKind::LeafExprNode;
    }
  };

} // end namespace clang

namespace clang {
  class PreorderAST {
  private:
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;
    bool Error;
    Node *Root;

    // Create a PreorderAST for the expression E.
    // @param[in] E is the sub expression to be added to a new node.
    // @param[in] Parent is the parent of the new node.
    void Create(Expr *E, Node *Parent = nullptr);

    // Add a new node to the AST.
    // @param[in] Node is the current node to be added.
    // @param[in] Parent is the parent of the node to be added.
    void AddNode(Node *N, Node *Parent);

    // Move the children (if any) of node N to its parent and then remove N.
    // @param[in] N is the current node.
    // @param[in] P is the parent of the node to be removed. P should be a
    // BinaryNode.
    void RemoveNode(Node *N, Node *P);

    // Recursively coalesce binary nodes having the same commutative and
    // associative operator.
    // @param[in] N is current node of the AST. Initial value is Root.
    // @param[in] Changed indicates whether a node was coalesced. We need this
    // to control when to stop recursive coalescing.
    void Coalesce(Node *N, bool &Changed);

    // Sort the children expressions in a binary node of the AST.
    // @param[in] N is current node of the AST. Initial value is Root.
    void Sort(Node *N);

    // Compare nodes N1 and N2 to sort them. This function is invoked by a
    // lambda which is passed to the llvm::sort function.
    // @param[in] N1 is the first node to compare.
    // @param[in] N2 is the second node to compare.
    // return A boolean indicating the relative ordering between N1 and N2.
    bool CompareNodes(const Node *N1, const Node *N2);

    // Constant fold integer expressions.
    // @param[in] N is current node of the AST. Initial value is Root.
    // @param[in] Changed indicates whether constant folding was done. We need
    // this to control when to stop recursive constant folding.
    void ConstantFold(Node *N, bool &Changed);

    // Normalize expressions which do not have any integer constants.
    // @param[in] N is current node of the AST. Initial value is Root.
    void NormalizeExprsWithoutConst(Node *N);

    // Check if the two AST nodes N1 and N2 are equal.
    // @param[in] N1 is the first node.
    // @param[in] N2 is the second node.
    // @return Returns a boolean indicating whether N1 and N2 are equal.
    bool IsEqual(Node *N1, Node *N2);

    // Set Error in case an error occurs during transformation of the AST.
    void SetError() { Error = true; }

    // Print the PreorderAST.
    // @param[in] N is the current node of the AST. Initial value is Root.
    void PrettyPrint(Node *N);

    // Cleanup the memory consumed by node N.
    // @param[in] N is the current node of the AST. Initial value is Root.
    void Cleanup(Node *N);

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
