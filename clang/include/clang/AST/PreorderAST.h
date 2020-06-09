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
  using Opcode = BinaryOperatorKind;

  class PreorderAST {

    class ASTData {
    private:
      Opcode opcode;
      SmallVector<Expr *, 4> operands;
      llvm::APSInt constants;

    public:
      ASTData(Opcode opc) : opcode(opc) {}

      void addOperand(Expr *op) { operands.push_back(op); }
      Opcode getOpcode() { return opcode; }
      size_t getOperandSize() { return operands.size(); }
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
      print(AST);
    }

    void insert(Expr *E, ASTNode *CurrNode, ASTNode *Parent = nullptr);
    void print(ASTNode *N);

    bool IsDeclOperand(Expr *E);
    Expr *IgnoreCasts(const Expr *E);
  };
}

#endif
