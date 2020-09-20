//=--ArrayBoundsInferenceConsumer.h-------------------------------*- C++-*-===//
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

#include "clang/Analysis/CFG.h"
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/ASTConsumer.h"

#include "ProgramInfo.h"

class LocalVarABVisitor;
class ConstraintResolver;

// This class handles determining bounds of global array variables.
// i.e., function parameters, structure fields and global variables.
class GlobalABVisitor: public clang::RecursiveASTVisitor<GlobalABVisitor> {
public:
  explicit GlobalABVisitor(ASTContext *C, ProgramInfo &I)
          : ParamInfo(nullptr), Context(C), Info(I) {}

  bool VisitRecordDecl(RecordDecl *RD);

  bool VisitFunctionDecl(FunctionDecl *FD);

  void SetParamHeuristicInfo(LocalVarABVisitor *LAB);

private:
  bool IsPotentialLengthVar(ParmVarDecl *PVD);
  LocalVarABVisitor *ParamInfo;
  ASTContext *Context;
  ProgramInfo &Info;
};

// This class handles determining bounds of function-local array variables.
// This class also keeps tracks of variables that are most-likely cannot be lengths.
// For example:
// Consider the expression: (x & y)
// Here, it is unlikely that variables x and y cannot be length variables
// because it is hard to imaging a variable used as length used in a bitwise AND.
class LocalVarABVisitor : public clang::RecursiveASTVisitor<LocalVarABVisitor> {

public:
  explicit LocalVarABVisitor(ASTContext *C, ProgramInfo &I)
  : Context(C), Info(I) {}

  bool HandleBinAssign(BinaryOperator *O);
  bool VisitDeclStmt(DeclStmt *S);
  bool VisitSwitchStmt(SwitchStmt *S);
  bool VisitBinaryOperator(BinaryOperator *O);
  bool VisitArraySubscriptExpr(ArraySubscriptExpr *E);
  bool isNonLengthParameter(ParmVarDecl *PVD);

private:
  void handleAssignment(BoundsKey LK, QualType LHSType, Expr *RHS);
  void addUsedParmVarDecl(Expr *CE);
  std::set<ParmVarDecl *> NonLengthParameters;
  ASTContext *Context;
  ProgramInfo &Info;
};

// Statement visitor that tries to find potential length variables of arrays
// based on the usage.
// Example:
// if (i < len) { ....arr[i]...}
// Here, we detect that len is a potential length of arr.
class LengthVarInference : public StmtVisitor<LengthVarInference> {
public:
  LengthVarInference(ProgramInfo &In, ASTContext *AC,
                     FunctionDecl *F);

  virtual ~LengthVarInference();

  void VisitStmt(Stmt *St);

  void VisitArraySubscriptExpr(ArraySubscriptExpr *ASE);

private:
  std::map<const Stmt *, CFGBlock *> StMap;
  ProgramInfo &I;
  ASTContext *C;
  FunctionDecl *FD;
  CFGBlock *CurBB;
  ControlDependencyCalculator *CDG;
  ConstraintResolver *CR;
  std::unique_ptr<CFG> Cfg;
};

void HandleArrayVariablesBoundsDetection(ASTContext *C, ProgramInfo &I);

// Add constraints based on heuristics to the parameters of the
// provided function.
void AddMainFuncHeuristic(ASTContext *C, ProgramInfo &I, FunctionDecl *FD);

#endif //_ARRAYBOUNDSINFERENCECONSUMER_H
