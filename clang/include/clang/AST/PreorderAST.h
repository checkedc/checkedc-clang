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
  using OpcodeTy = BinaryOperator::Opcode;
  using VarTy = std::string;
  using VarListTy = std::vector<VarTy>;
  using ConstTy = llvm::APSInt;

  class PreorderAST {
    class ASTNode {
    public:
      OpcodeTy opcode;
      VarListTy variables;
      ConstTy constant;
      bool hasConstant;
      ASTNode *left, *right, *parent;
      
      ASTNode(ASTContext &Ctx, ASTNode *Parent = nullptr) :
        opcode(BO_Add), hasConstant(false),
        left(nullptr), right(nullptr), parent(Parent) {
          llvm::APSInt Zero(Ctx.getTypeSize(Ctx.IntTy), 0);
          constant = Zero;
        }

      void addVar(VarTy V) { variables.push_back(V); }

      void setOpcode(OpcodeTy Opc) { opcode = Opc; }

      bool isLeafNode() { return !left && !right; }
    };

  private:
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;
    ASTNode *AST;

  public:
    PreorderAST(ASTContext &Ctx, Expr *E) :
      Ctx(Ctx), Lex(Lexicographic(Ctx, nullptr)),
      OS(llvm::outs()) {

      AST = new ASTNode(Ctx);
      insert(AST, E);
      coalesce(AST);
      sort(AST);
      normalize(AST);
      print(AST);
    }

    void insert(ASTNode *N, Expr *E, ASTNode *Parent = nullptr);
    void coalesce(ASTNode *N);
    void coalesceConst(ASTNode *N, llvm::APSInt IntVal);
    void sort(ASTNode *N);
    void normalize(ASTNode *N);
    void print(ASTNode *N);
    Result compare(PreorderAST &PT);
    Result compare(ASTNode *N1, ASTNode *N2);

    Expr *IgnoreCasts(const Expr *E);
    bool IsDeclOperand(Expr *E);
    DeclRefExpr *GetDeclOperand(Expr *E);
  };
}

#endif
