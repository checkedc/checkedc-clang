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
  // Each node of the PreorderAST has a variable-count list which stores the
  // name of each variable along with a count of the number of times the
  // variable appears in the node.
  struct VarTy {
    std::string name;
    llvm::APSInt count;
  };

  // Each node of the PreorderAST contains 3 fields:
  // - opcode: The opcode for the node.
  // - variable-count list: A list of variables and their counts.
  // - constant value: All constants occurring in the node are constant folded.

  // For example, consider an expression E = a + 3 + b + 4 + a
  // A node of the PreorderAST for E would contain the following:
  // - opcode: +
  // - variable-count list: [a:2, b:1]
  // - constant value: 3 + 4 = 7

  using VarListTy = std::vector<VarTy>;
  using OpcodeTy = BinaryOperator::Opcode;
  using ConstTy = llvm::APSInt;
  using Result = Lexicographic::Result;

  class PreorderAST {
    class ASTNode {
    public:
      ASTContext &Ctx;
      OpcodeTy opcode;
      VarListTy variables;
      ConstTy constant;
      ASTNode *left, *right, *parent;
      bool HasConstant;
      
      ASTNode(ASTContext &Ctx, ASTNode *Parent = nullptr) :
        Ctx(Ctx), opcode(BO_Add),
        left(nullptr), right(nullptr), parent(Parent),
        HasConstant(false) {
          // We initialize the constant value for each node to 0. So we need a
          // way to indicate the absence of a constant. So we have a boolean
          // HasConstant to indicate whether a node has a constant or not.
          constant = getConstVal(0);
        }

      llvm::APSInt getConstVal(unsigned Val) {
      return llvm::APSInt(llvm::APInt(Ctx.getTypeSize(Ctx.IntTy), Val));
      }

      void addVar(std::string name) {
        // We initialize the count of each variable in a node to 1.
        variables.push_back(VarTy {name, getConstVal(1)});
      }

      // Is the operator commutative and associative?
      bool isOpCommutativeAndAssociative() {
        return opcode == BO_Add ||
               opcode == BO_Mul;
      }
    };

  private:
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;
    bool HasError;
    ASTNode *AST;

  public:
    PreorderAST(ASTContext &Ctx, Expr *E) :
      Ctx(Ctx), Lex(Lexicographic(Ctx, nullptr)),
      OS(llvm::outs()), HasError(false) {

      HasError = false;
      AST = new ASTNode(Ctx);
      insert(AST, E);
      normalize(AST, HasError);
    }

    // Create an preorder AST from the expression E.
    // @param[in] N is the current node of the AST.
    // @param[in] E is the sub expression which needs to be added to N.
    // @param[in] Parent is the parent node for N.
    void insert(ASTNode *N, Expr *E, ASTNode *Parent = nullptr);

    // Normalize the input expression through a series of transforms on the
    // preorder AST.
    // @param[in] N is the root of the AST.
    // @param[out] HasError is populated if an error is encountered during
    // normalization.
    void normalize(ASTNode *N, bool &HasError);

    // Sort the variables in a node of the AST.
    // @param[in] N is the root of the AST.
    // @param[out] HasError is populated if an error is encountered during
    // sorting.
    void sort(ASTNode *N, bool &HasError);

    // Check if the two ASTs N1 and N2 are equal.
    // @param[in] N1 is the first AST.
    // @param[in] N2 is the second AST.
    // @return Returns a boolean indicating whether N1 and N2 are equal.
    bool isEqual(ASTNode *N1, ASTNode *N2);

    // Compare the current AST with the given AST. This in turn, invokes
    // isEqual(N1, N2);
    // @param[in] this is the first AST.
    // @param[in] PT is the second AST.
    // @return Returns a value of type Lexicographic::Result indicating whether
    // the two ASTs are equal or not. 
    Result compare(PreorderAST &PT);

    // Cleanup the memory consumed by the AST.
    // @param[in] N is the root node of the AST.
    void cleanup(ASTNode *N);

    // Cleanup the memory consumed by the AST. This is intended to be called
    // from outside the PreorderAST class.
    void cleanup();

    // Check if an error has occurred during normalization of the expression.
    // @return Whether an error has occurred or not.
    bool hasError() {
      return HasError;
    }

    // Print the preorder AST.
    // @param[in] N is the root node of the AST.
    void print(ASTNode *N);

    // Invoke IgnoreValuePreservingOperations to strip off casts.
    // @param[in] E is the expression whose casts must be stripped.
    // @return E with casts stripped off.
    Expr *IgnoreCasts(const Expr *E);

    // A DeclRefExpr can be a reference either to an array subscript (in which
    // case it is wrapped around a ArrayToPointerDecay cast) or to a pointer
    // dereference (in which case it is wrapped around an LValueToRValue cast).
    // @param[in] An expression E.
    // @return Whether E is an expression containing a reference to an array
    // subscript or a pointer dereference.
    bool IsDeclOperand(Expr *E);

    // Get the DeclRefExpr from an expression E.
    // @param[in] An expression E which is known to be either an LValueToRValue
    // cast or an ArrayToPointerDecay cast.
    // @return The DeclRefExpr from the expression E.
    DeclRefExpr *GetDeclOperand(Expr *E);
  };
}

#endif
