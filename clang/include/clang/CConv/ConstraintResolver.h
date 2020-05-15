//=--ConstraintResolver.h-----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Class that helps in resolving constraints for various expressions.
//===----------------------------------------------------------------------===//

#ifndef _CONSTRAINTRESOLVER_H
#define _CONSTRAINTRESOLVER_H

#include "clang/AST/ASTConsumer.h"

#include "ProgramInfo.h"

using namespace llvm;
using namespace clang;

// Class that handles building constraints from various AST artifacts.
class ConstraintResolver {

public:
  ConstraintResolver(ProgramInfo &I, ASTContext *C) : Info(I), Context(C) {
    GlobalRValueCons.clear();
  }

  // Special-case handling for decl introductions. For the moment this covers:
  //  * void-typed variables
  //  * va_list-typed variables
  void specialCaseVarIntros(ValueDecl *D, bool FuncCtx = false);

  bool handleFuncCall(CallExpr *CA, QualType LhsType);

  void constraintAllCVarsToWild(std::set<ConstraintVariable*> &CSet,
                                std::string rsn,
                                Expr *AtExpr = nullptr);

  std::set<ConstraintVariable *>  getExprConstraintVars(
      Expr                            *E,
      QualType                   LhsType,
      bool                            Ifc,
      bool             NonEmptyCons = false);

  // This is a bit of a hack. What we need to do is traverse the AST in a
  // bottom-up manner, and, for a given expression, decide which singular,
  // if any, constraint variable is involved in that expression. However,
  // in the current version of clang (3.8.1), bottom-up traversal is not
  // supported. So instead, we do a manual top-down traversal, considering
  // the different cases and their meaning on the value of the constraint
  // variable involved. This is probably incomplete, but, we're going to
  // go with it for now.
  //
  // V is (currentVariable, baseVariable, limitVariable)
  // E is an expression to recursively traverse.
  //
  // Returns true if E resolves to a constraint variable q_i and the
  // currentVariable field of V is that constraint variable. Returns false if
  // a constraint variable cannot be found.
  // ifc mirrors the inFunctionContext boolean parameter to getVariable.
  std::set<ConstraintVariable *>  getExprConstraintVars(
      std::set<ConstraintVariable *> &LHSConstraints,
      Expr                            *E,
      std::set<ConstraintVariable *> &RvalCons,
      QualType                   LhsType,
      bool                    &IsAssigned,
      bool                            Ifc);

  void constrainLocalAssign(Stmt *TSt, Expr *LHS, Expr *RHS, ConsAction CAction);

  void constrainLocalAssign(Stmt *TSt, DeclaratorDecl *D, Expr *RHS,
                            ConsAction CAction = Same_to_Same);

private:
  ProgramInfo &Info;
  ASTContext *Context;
  // These are temporary R-Value Constraints, that will be created to handle
  // R-Value expressions, such as constants and Function Calls.
  static std::set<ConstraintVariable *> GlobalRValueCons;

  std::set<ConstraintVariable *> getWildPVConstraint();
};

#endif // _CONSTRAINTRESOLVER_H
