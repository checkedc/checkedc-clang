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
  class LeafExprNode;

  class Node {
  public:
    // Nodes with two different kinds are sorted according to the order in
    // which their kinds appear in this enum.
    enum class NodeKind {
      BinaryOperatorNode,
      UnaryOperatorNode,
      MemberNode,
      ImplicitCastNode,
      LeafExprNode
    };

    NodeKind Kind;
    Node *Parent;

    Node(NodeKind Kind, Node *Parent) :
      Kind(Kind), Parent(Parent) {
        if (Parent)
          assert(!isa<LeafExprNode>(Parent) &&
                 "Parent node cannot be a LeafExprNode");
      }
  };

  class BinaryOperatorNode : public Node {
  public:
    BinaryOperator::Opcode Opc;
    // A BinaryOperatorNode representing a commutative and associative binary
    // operation may have more than two children because of coalescing.
    // Ex: a + (b + c) will be represented by one BinaryOperatorNode for +
    // with three children nodes for a, b and c after coalescing.
    llvm::SmallVector<Node *, 2> Children;

    BinaryOperatorNode(BinaryOperator::Opcode Opc, Node *Parent) :
      Node(NodeKind::BinaryOperatorNode, Parent),
      Opc(Opc) {}

    static bool classof(const Node *N) {
      return N->Kind == NodeKind::BinaryOperatorNode;
    }

    // Is the operator commutative and associative?
    bool IsOpCommutativeAndAssociative() {
      return Opc == BO_Add || Opc == BO_Mul;
    }
  };

  class UnaryOperatorNode : public Node {
  public:
    UnaryOperator::Opcode Opc;
    Node *Child;

    UnaryOperatorNode(UnaryOperator::Opcode Opc, Node *Parent) :
      Node(NodeKind::UnaryOperatorNode, Parent),
      Opc(Opc) {}

    static bool classof(const Node *N) {
      return N->Kind == NodeKind::UnaryOperatorNode;
    }
  };

  class MemberNode : public Node {
  public:
    Node *Base = nullptr;
    ValueDecl *Field = nullptr;
    bool IsArrow;

    MemberNode(ValueDecl *Field, bool IsArrow, Node *Parent) :
      Node(NodeKind::MemberNode, Parent),
      Field(Field), IsArrow(IsArrow) {}

    static bool classof(const Node *N) {
      return N->Kind == NodeKind::MemberNode;
    }
  };

  class ImplicitCastNode : public Node {
  public:
    CastKind CK;
    Node *Child;

    ImplicitCastNode(CastKind CK, Node *Parent) :
      Node(NodeKind::ImplicitCastNode, Parent),
      CK(CK) {}

    static bool classof(const Node *N) {
      return N->Kind == NodeKind::ImplicitCastNode;
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

    // Create a BinaryOperatorNode with an addition operator and two children
    // (E and 0), and attach the created BinaryOperatorNode to the Parent node.
    // @param[in] E is the expression that is one of the two children of
    // the created BinaryOperatorNode (the other child is 0).
    // @param[in] Parent is the parent of the created BinaryOperatorNode.
    void AddZero(Expr *E, Node *Parent);

    // Add a new node to the AST.
    // @param[in] Node is the current node to be added.
    // @param[in] Parent is the parent of the node to be added.
    void AddNode(Node *N, Node *Parent);

    // Coalesce the BinaryOperatorNode B with its parent. This involves moving
    // the children (if any) of node B to its parent and then removing B.
    // @param[in] B is the current node. B should be a BinaryOperatorNode.
    void CoalesceNode(BinaryOperatorNode *B);

    // Determines if a BinaryOperatorNode could be coalesced into its parent.
    // @param[in] B is the current node. B should be a BinaryOperatorNode.
    // @return Return true if B can be coalesced into its parent, false
    // otherwise.
    bool CanCoalesceNode(BinaryOperatorNode *B);

    // Recursively coalesce BinaryOperatorNodes having the same commutative
    // and associative operator.
    // @param[in] N is current node of the AST. Initial value is Root.
    // @param[in] Changed indicates whether a node was coalesced. We need this
    // to control when to stop recursive coalescing.
    void Coalesce(Node *N, bool &Changed);

    // Recursively descend the PreorderAST to sort the children of all
    // BinaryOperatorNodes if the binary operator is commutative.
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

    // Constant fold integer expressions within a BinaryOperatorNode.
    // @param[in] N is current node of the AST.
    // @param[in] Changed indicates whether constant folding was done. We need
    // this to control when to stop recursive constant folding.
    void ConstantFoldOperator(BinaryOperatorNode *N, bool &Changed);

    // Get the deref offset from the DerefExpr. The offset represents the
    // possible amount by which the bounds of an ntptr could be widened.
    // @param[in] UpperExpr is the upper bounds expr for the ntptr.
    // @param[in] DerefExpr is the dereferenced expr for the ntptr.
    // @param[out] Offset is the offset from the base by which the pointer is
    // dereferenced.
    // return Returns a boolean indicating whether a valid offset exists. True
    // means a valid offset was found and is present in the "Offset" parameter.
    // False means a valid offset was not found.
    bool GetDerefOffset(Node *UpperExpr, Node *DerefExpr,
                        llvm::APSInt &Offset);

    // Lexicographically compare two AST nodes N1 and N2.
    // @param[in] N1 is the first node.
    // @param[in] N2 is the second node.
    // @return Returns a Lexicographic::Result indicating the comparison
    // of N1 and N2.
    Result Compare(const Node *N1, const Node *N2) const;

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

    // Get the offset by which a pointer is dereferenced. This function is
    // intended to be called from outside this class.
    // @param[in] this is the first AST.
    // @param[in] P is the second AST.
    // @param[out] Offset is the dereference offset.
    // @return Returns a bool indicating whether a valid dereference offset
    // exists.
    bool GetDerefOffset(PreorderAST &P, llvm::APSInt &Offset) {
      return GetDerefOffset(/*UpperExpr*/ Root, /*DerefExpr*/ P.Root, Offset);
    }

    // Lexicographically compare the two ASTs. This is intended to be called
    // from outside this class and invokes Compare on the root nodes of the two
    // ASTs to recursively compare the AST nodes.
    // @param[in] this is the first AST.
    // @param[in] P is the second AST.
    // @return Returns a Lexicographic::Result indicating the comparison between
    // the two ASTs.
    Result Compare(const PreorderAST P) const { return Compare(Root, P.Root); }

    // Check if an error has occurred during transformation of the AST. This
    // is intended to be called from outside this class to check if an error
    // has occurred during comparison of expressions.
    // @return Whether an error has occurred or not.
    bool GetError() { return Error; }

    // Cleanup the memory consumed by the AST. This is intended to be called
    // from outside this class and invokes Cleanup on the root node which
    // recursively deletes the AST.
    void Cleanup() { Cleanup(Root); }

    bool operator<(PreorderAST &Other) const {
      return Compare(Other) == Result::LessThan;
    }
    bool operator==(PreorderAST &Other) const {
      return Compare(Other) == Result::Equal;
    }
  };

  // This comparison allows PreorderASTs to be sorted lexicographically.
  struct PreorderASTComparer {
    bool operator()(PreorderAST *A, PreorderAST *B) const {
      return *A < *B;
    }
  };

} // end namespace clang
#endif
