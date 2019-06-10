//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This is an ASTConsumer that tries to infer the CheckedC style bounds
// for identified array variables.
//===----------------------------------------------------------------------===//

#ifndef _ARRAYBOUNDSINFERENCECONSUMER_H
#define _ARRAYBOUNDSINFERENCECONSUMER_H

#include "clang/AST/ASTConsumer.h"

#include "ProgramInfo.h"


class LocalVarABVisitor: public clang::RecursiveASTVisitor<LocalVarABVisitor> {
public:
  explicit LocalVarABVisitor(ASTContext *C, ProgramInfo &I)
          : Context(C), Info(I) {}

  // handles assignment expression.
  bool VisitBinAssign(BinaryOperator *O);


  bool VisitDeclStmt(DeclStmt *S);
private:
  // check if the provided expression is a call
  // to known memory allocators.
  // if yes, return true along with the argument used as size
  // assigned to the second paramter i.e., sizeArgument
  bool isAllocatorCall(Expr *currExpr, Expr **sizeArgument);

  // check if expression is a simple local variable
  // i.e., ptr = .
  // if yes, return the referenced local variable as the return
  // value of the argument.
  bool isExpressionSimpleLocalVar(Expr *toCheck, Decl **targetDecl);

  // remove implicit casts added by clang to the AST
  Expr *removeImpCasts(Expr *toConvert);

  ASTContext *Context;
  ProgramInfo &Info;
  static std::set<std::string> AllocatorFunctionNames;
};

void HandleArrayVariablesBoundsDetection(ASTContext *C, ProgramInfo &I);

#endif //_ARRAYBOUNDSINFERENCECONSUMER_H
