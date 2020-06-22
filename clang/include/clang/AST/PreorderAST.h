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
  // Name of each variable along with a count of the number of times the
  // variable appears in the node.
  struct VarTy {
    std::string Name;
    llvm::APSInt Count;
  };

  // Each node of the PreorderAST contains 3 fields:
  // - Opcode: The Opcode for the node.
  // - variable-count list: A list of variables and their counts.
  // - Constant value: All Constants occurring in the node are Constant folded.

  // For example, consider an expression E = a + 3 + b + 4 + a
  // A node of the PreorderAST for E would contain the following:
  // - Opcode: +
  // - variable-count list: [a:2, b:1]
  // - Constant value: 3 + 4 = 7

  using VarListTy = std::vector<VarTy>;
  using OpcodeTy = BinaryOperator::Opcode;
  using ConstTy = llvm::APSInt;
  using Result = Lexicographic::Result;

  class PreorderAST {
    class ASTNode {
    public:
      ASTContext &Ctx;
      OpcodeTy Opcode;
      VarListTy Variables;
      ConstTy Constant;
      ASTNode *Left, *Right, *Parent;
      bool HasConstant;
      
      ASTNode(ASTContext &Ctx, ASTNode *Parent = nullptr) :
        Ctx(Ctx), Opcode(BO_Add),
        Left(nullptr), Right(nullptr), Parent(Parent),
        HasConstant(false) {
          // We initialize the Constant value for each node to 0. So we need a
          // way to indicate the absence of a Constant. So we have a boolean
          // HasConstant to indicate whether a node has a Constant or not.
          Constant = GetConstVal(0);
        }

      llvm::APSInt GetConstVal(unsigned Val) {
        return llvm::APSInt(llvm::APInt(Ctx.getTypeSize(Ctx.IntTy), Val));
      }

      void AddVar(std::string Name) {
        // We initialize the count of each variable in a node to 1.
        Variables.push_back(VarTy {Name, GetConstVal(1)});
      }

      // Is the operator commutative and associative?
      bool IsOpCommutativeAndAssociative() {
        return Opcode == BO_Add ||
               Opcode == BO_Mul;
      }
    };

  private:
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;
    bool Error;
    ASTNode *AST;

    // Create a preorder AST from the expression E.
    // @param[in] N is the current node of the AST.
    // @param[in] E is the sub expression which needs to be added to N.
    // @param[in] Parent is the Parent node for N.
    void Create(ASTNode *N, Expr *E, ASTNode *Parent = nullptr);

    // Normalize the input expression through a series of transforms on the
    // preorder AST. The Error field is set if an error is encountered during
    // transformation of the AST.
    // @param[in] N is the root of the AST.
    void Normalize(ASTNode *N);

    // Sort the variables in a node of the AST. The Error field is set if an
    // error is encountered during transformation of the AST.
    // @param[in] N is the root of the AST.
    void Sort(ASTNode *N);

    // Check if the two ASTs N1 and N2 are equal.
    // @param[in] N1 is the first AST.
    // @param[in] N2 is the second AST.
    // @return Returns a boolean indicating whether N1 and N2 are equal.
    bool IsEqual(ASTNode *N1, ASTNode *N2);

    // Cleanup the memory consumed by the AST.
    // @param[in] N is the root node of the AST.
    void Cleanup(ASTNode *N);

    // Set Error in case an error occurs during transformation of the AST.
    // @param[in] Err is the value to be set for the Error field.
    void SetError(bool Err) {
      Error = Err;
    }

    // Print the preorder AST.
    // @param[in] N is the root node of the AST.
    void PrettyPrint(ASTNode *N);

    // A DeclRefExpr can be a reference either to an array subscript (in which
    // case it is wrapped around a ArrayToPointerDecay cast) or to a pointer
    // dereference (in which case it is wrapped around an LValueToRValue cast).
    // @param[in] An expression E.
    // @param[out] D is populated with the DeclRefExpr if E contains a
    // DeclRefExpr.
    // @return A bool indicating whether E is an expression containing a
    // reference to an array subscript or a pointer dereference.
    bool IsDeclOperand(Expr *E, DeclRefExpr *&D);

  public:
    PreorderAST(ASTContext &Ctx, Expr *E) :
      Ctx(Ctx), Lex(Lexicographic(Ctx, nullptr)),
      OS(llvm::outs()), Error(false) {

      AST = new ASTNode(Ctx);
      Create(AST, E);
      Normalize(AST);
    }

    // Check if an error has occurred during transformation of the AST.
    // @return Whether an error has occurred or not.
    bool GetError() {
      return Error;
    }

    // Compare the current AST with the given AST. This in turn, invokes
    // isEqual(N1, N2);
    // @param[in] this is the first AST.
    // @param[in] PT is the second AST.
    // @return Returns a value of type Lexicographic::Result indicating whether
    // the two ASTs are equal or not.
    Result Compare(PreorderAST &PT);

    // Cleanup the memory consumed by the AST. This is intended to be called
    // from outside the PreorderAST class.
    void Cleanup();
  };
}

#endif
