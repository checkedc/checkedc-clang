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
  class Expr;
  using Opcode = BinaryOperator::Opcode;
  using Operands = SmallVector<Expr *, 4>;
  using Constants = llvm::APSInt;

  class PreorderAST {

    class ASTData {
    private:
      Opcode opcode;
      Operands operands;
      Constants constants;

    public:
      ASTData(Opcode opc) : opcode(opc) {}

      Opcode getOpcode() { return opcode; }
      size_t getNumOperands() { return operands.size(); }
      void addOperand(Expr *op) { operands.push_back(op); }
      Operands getOperands() { return operands; }
    };

    class ASTNode {
    public:
      ASTData *data;
      ASTNode *left, *right, *parent;
      
      ASTNode(ASTNode *p = nullptr) :
        data(nullptr), left(nullptr), right(nullptr), parent(p) {}

      ~ASTNode() {}
    };

  private:
    ASTContext &Ctx;
    Lexicographic Lex;
    llvm::raw_ostream &OS;
    ASTNode *AST;

  public:
    PreorderAST(ASTContext &Ctx, const Expr *E) :
      Ctx(Ctx), Lex(Lexicographic(Ctx, nullptr)), OS(llvm::outs()) {

      AST = new ASTNode();
      insert(const_cast<Expr *>(E), AST);
      coalesce(AST);
      print(AST);
    }

    void insert(Expr *E, ASTNode *CurrNode, ASTNode *Parent = nullptr);
    void coalesce(ASTNode *N);
    void print(ASTNode *N);

    bool isLeafNode(ASTNode *N) {
      return N && !N->left && !N->right;
    }

    bool hasData(ASTNode *N) {
      return N && N->data && N->data->getNumOperands();
    }

    bool IsDeclOperand(Expr *E);
    Expr *IgnoreCasts(const Expr *E);
  };
}

#endif
