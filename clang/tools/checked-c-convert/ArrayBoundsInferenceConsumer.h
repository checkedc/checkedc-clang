//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This is an ASTConsumer that tries to infer the CheckedC style bounds
// for identified array variables.
//===----------------------------------------------------------------------===//

#ifndef _ARRAYBOUNDSINFERENCECONSUMER_H
#define _ARRAYBOUNDSINFERENCECONSUMER_H

#include "clang/AST/ASTConsumer.h"

#include "ProgramInfo.h"

class LocalVarABVisitor;

// This class handles determining bounds of global array variables.
// i.e., function parameters, structure fields and global variables.
class GlobalABVisitor: public clang::RecursiveASTVisitor<GlobalABVisitor> {
public:
  explicit GlobalABVisitor(ASTContext *C, ProgramInfo &I)
          : Context(C), Info(I), ParamInfo(nullptr) {}

  bool VisitRecordDecl(RecordDecl *RD);

  bool VisitFunctionDecl(FunctionDecl *FD);

  void SetParamHeuristicInfo(LocalVarABVisitor *LAB);

private:
  bool IsPotentialLengthVar(ParmVarDecl* PVD);
  LocalVarABVisitor *ParamInfo;
  ASTContext *Context;
  ProgramInfo &Info;
};

// This class handles determining bounds of function-local array variables.
class LocalVarABVisitor : public clang::RecursiveASTVisitor<LocalVarABVisitor> {

public:
  explicit LocalVarABVisitor(ASTContext *C, ProgramInfo &I)
  : Context(C), Info(I) {}

  bool VisitBinAssign(BinaryOperator *O);
  bool VisitDeclStmt(DeclStmt *S);
  bool VisitSwitchStmt(SwitchStmt *S);
  bool VisitIfStmt(IfStmt *IFS);
  bool isNonLengthParameter(ParmVarDecl* PVD);

private:
  void addUsedParmVarDecl(Expr *CE);
  std::set<ParmVarDecl*> NonLengthParameters;
  ASTContext *Context;
  ProgramInfo &Info;
};

void HandleArrayVariablesBoundsDetection(ASTContext *C, ProgramInfo &I);

// Add constraints based on heuristics to the parameters of the
// provided function.
void AddArrayHeuristics(ASTContext *C, ProgramInfo &I, FunctionDecl *FD);

#endif //_ARRAYBOUNDSINFERENCECONSUMER_H
