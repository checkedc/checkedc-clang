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

    virtual ~Node() { }

    // Recursively coalesce BinaryOperatorNodes having the same commutative
    // and associative operator.
    // @param[in] this is the current node of the AST.
    // @param[in] Error indicates whether an error occurred during coalescing.
    // @return Returns true if this node was deleted. The node is deleted if
    // it is coalesced into its parent.
    virtual bool Coalesce(bool &Error) = 0;

    // Recursively descend a Node to sort the children of all
    // BinaryOperatorNodes if the binary operator is commutative.
    // @param[in] this is the current node of the AST.
    // @param[in] Lex is used to lexicographically compare Exprs and Decls
    // that occur within nodes.
    virtual void Sort(Lexicographic Lex) = 0;

    // Constant fold integer expressions.
    // @param[in] this is the current node of the AST.
    // @param[in] Error indicates whether an error occurred during constant
    // folding.
    // @param[in] Ctx is used to create constant expressions.
    // @return Returns true if this node was deleted. The node is deleted if
    // it is coalesced into its parent at the end of constant folding.
    virtual bool ConstantFold(bool &Error, ASTContext &Ctx) = 0;

    // Compare nodes according to their kind.
    // @param[in] this is the current node of the AST.
    // @param[in] Other is the node to compare to this.
    // @return Returns a Lexicographic::Result indicating the comparison
    // between this and Other according to their node kinds.
    Result CompareKinds(const Node *Other) const {
      if (Kind < Other->Kind)
        return Result::LessThan;
      if (Kind > Other->Kind)
        return Result::GreaterThan;
      return Result::Equal;
    }

    // Compare two nodes lexicographically.
    // @param[in] this is the current node of the AST.
    // @param[in] Other the node to compare to this.
    // @param[in] Lex is used to lexicographically compare Exprs and Decls
    // that occur within nodes.
    // @return Returns a Lexicographic::Result indicating the comparison
    // between this and Other.
    virtual Result Compare(const Node *Other, Lexicographic Lex) const = 0;

    // Print the node.
    // @param[in] this is the current node of the AST.
    virtual void PrettyPrint(llvm::raw_ostream &OS, ASTContext &Ctx) const = 0;

    // Cleanup the memory consumed by this node.
    // @param[in] this is the current node of the AST.
    virtual void Cleanup() {
      delete this;
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

    // Determines if the BinaryOperatorNode could be coalesced into its parent.
    // @param[in] this is the current node.
    // @return Returns true if this can be coalesced into its parent, false
    // otherwise.
    bool CanCoalesce();
    bool Coalesce(bool &Error);
    void Sort(Lexicographic Lex);
    bool ConstantFold(bool &Error, ASTContext &Ctx);
    Result Compare(const Node *Other, Lexicographic Lex) const;
    void PrettyPrint(llvm::raw_ostream &OS, ASTContext &Ctx) const;
    void Cleanup();
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

    bool Coalesce(bool &Error);
    void Sort(Lexicographic Lex);
    bool ConstantFold(bool &Error, ASTContext &Ctx);
    Result Compare(const Node *Other, Lexicographic Lex) const;
    void PrettyPrint(llvm::raw_ostream &OS, ASTContext &Ctx) const;
    void Cleanup();
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

    bool Coalesce(bool &Error);
    void Sort(Lexicographic Lex);
    bool ConstantFold(bool &Error, ASTContext &Ctx);
    Result Compare(const Node *Other, Lexicographic Lex) const;
    void PrettyPrint(llvm::raw_ostream &OS, ASTContext &Ctx) const;
    void Cleanup();
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

    bool Coalesce(bool &Error);
    void Sort(Lexicographic Lex);
    bool ConstantFold(bool &Error, ASTContext &Ctx);
    Result Compare(const Node *Other, Lexicographic Lex) const;
    void PrettyPrint(llvm::raw_ostream &OS, ASTContext &Ctx) const;
    void Cleanup();
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

    bool Coalesce(bool &Error);
    void Sort(Lexicographic Lex);
    bool ConstantFold(bool &Error, ASTContext &Ctx);
    Result Compare(const Node *Other, Lexicographic Lex) const;
    void PrettyPrint(llvm::raw_ostream &OS, ASTContext &Ctx) const;
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

    // Create a BinaryOperatorNode for the expression E.
    // @param[in] E is the expression whose LHS and RHS subexpressions
    // will be added to a new node.
    // @param[in] Parent is the parent of the new node.
    void CreateBinaryOperator(BinaryOperator *E, Node *Parent);

    // Create a UnaryOperatorNode or a LeafExprNode for the expression E.
    // @param[in] E is the expression that is used to create a new node.
    // @param[in] Parent is the parent of the new node.
    void CreateUnaryOperator(UnaryOperator *E, Node *Parent);

    // Create a UnaryOperatorNode with the dereference operator and the
    // subexpression e1 + e2, given an array subscript expression e1[e2].
    // @param[in] E is the expression that is used to create a new node.
    // @param[in] Parent is the parent of the new node.
    void CreateArraySubscript(ArraySubscriptExpr *E, Node *Parent);

    // Create a MemberNode for the expression E.
    // @param[in] E is the expression whose Base and Field will be added to
    // a new node.
    // @param[in] Parent is the parent of the new node.
    void CreateMember(MemberExpr *E, Node *Parent);

    // Create an ImplicitCastNode for the expression.
    // @param[in] E is the expression whose CastKind and sub expression will
    // be added to a new node.
    // @param[in] Parent is the parent of the new node.
    void CreateImplicitCast(ImplicitCastExpr *E, Node *Parent);

    // Create a BinaryOperatorNode with an addition operator and two children
    // (E and 0), and attach the created BinaryOperatorNode to the Parent node.
    // This method is used to maintain an invariant that an expression `e` is
    // equivalent to `e + 0`. This invariant must hold when:
    // 1. `e` is the root expression that is used to create the PreorderAST.
    // 2. `e` is the subexpression of a dereference expression. For example:
    //   a. `*e` and `*(e + 0)` must have the same canonical form.
    //   b. `e1[e2]` is equivalent to `*(e1 + e2)`, so `*(e1 + e2)` and
    //       `*(e1 + e2 + 0)` must have the same canonical form.
    //   c. `e->f`, `*e.f`, `(e + 0)->f`, `*(e + 0).f`, and `e[0].f` must have
    //       the same canonical form.
    // @param[in] E is the expression that is one of the two children of
    // the created BinaryOperatorNode (the other child is 0).
    // @param[in] Parent is the parent of the created BinaryOperatorNode.
    void AddZero(Expr *E, Node *Parent);

    // Attach a new node to the AST. The node N is attached to the Parent node.
    // @param[in] N is the current node to be attached.
    // @param[in] Parent is the parent of the node to be attached.
    void AttachNode(Node *N, Node *Parent);

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

    // Get the integer difference between expressions.
    // @param[in] E1 is the first expression.
    // @param[in] E2 is the second expression.
    // @param[out] Offset is the integer difference between E1 and E2.
    // @return Returns a boolean indicating whether the expressions are
    // comparable. True means the expressions are comparable and their integer
    // difference is present in the "Offset" parameter.  False means the
    // expressions are not comparable.
    bool GetExprIntDiff(Node *E1, Node *E2, llvm::APSInt &Offset);

    // Set Error in case an error occurs during transformation of the AST.
    void SetError() { Error = true; }

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

    // Get the integer difference between two expressions represented as
    // preorder ASTs. This function is intended to be called from outside this
    // class.
    // @param[in] this is the first AST.
    // @param[in] P is the second AST.
    // @param[out] Offset is the integer difference.
    // @return Returns a bool indicating whether the two expressions are
    // comparable.
    bool GetExprIntDiff(PreorderAST &P, llvm::APSInt &Offset) {
      return GetExprIntDiff(Root, P.Root, Offset);
    }

    // Lexicographically compare the two ASTs. This is intended to be called
    // from outside this class and invokes Compare on the root nodes of the two
    // ASTs to recursively compare the AST nodes.
    // @param[in] this is the first AST.
    // @param[in] P is the second AST.
    // @return Returns a Lexicographic::Result indicating the comparison between
    // the two ASTs.
    Result Compare(const PreorderAST P) const {
      return Root->Compare(P.Root, Lex);
    }

    // Check if an error has occurred during transformation of the AST. This
    // is intended to be called from outside this class to check if an error
    // has occurred during comparison of expressions.
    // @return Whether an error has occurred or not.
    bool GetError() { return Error; }

    void PrettyPrint(llvm::raw_ostream &OS, ASTContext &Ctx) const {
      Root->PrettyPrint(OS, Ctx);
    }

    // Cleanup the memory consumed by the AST. This is intended to be called
    // from outside this class and invokes Cleanup on the root node which
    // recursively deletes the AST.
    void Cleanup() { Root->Cleanup(); }

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
