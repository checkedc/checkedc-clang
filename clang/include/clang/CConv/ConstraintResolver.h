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
  ConstraintResolver(ProgramInfo &I, ASTContext *C) : Info(I), Context(C) { }

  virtual ~ConstraintResolver();

  void constraintAllCVarsToWild(const CVarSet &CSet, const std::string &Rsn,
                                Expr *AtExpr = nullptr);
  void constraintCVarToWild(CVarOption CVar, const std::string &Rsn,
                            Expr *AtExpr = nullptr);

  // Returns a set of ConstraintVariables which represent the result of
  // evaluating the expression E. Will explore E recursively, but will
  // ignore parts of it that do not contribute to the final result
  CVarSet getExprConstraintVars(Expr *E);

  // Handle assignment of RHS expression to LHS expression using the
  // given action.
  void constrainLocalAssign(Stmt *TSt, Expr *LHS, Expr *RHS,
                            ConsAction CAction);

  // Handle the assignment of RHS to the given declaration.
  void constrainLocalAssign(Stmt *TSt, DeclaratorDecl *D, Expr *RHS,
                            ConsAction CAction = Same_to_Same);

  // Check if the set contains any valid constraints.
  bool containsValidCons(const CVarSet &CVs);
  bool isValidCons(ConstraintVariable *CV);
  // Try to get the bounds key from the constraint variable set.
  bool resolveBoundsKey(const CVarSet &CVs, BoundsKey &BK);
  bool resolveBoundsKey(CVarOption CV, BoundsKey &BK);

  static bool canFunctionBeSkipped(const std::string &FN);

private:
  ProgramInfo &Info;
  ASTContext *Context;

  CVarSet handleDeref(CVarSet T);

  CVarSet getInvalidCastPVCons(CastExpr *E);

  // Update a PVConstraint with one additional level of indirection
  PVConstraint *addAtom(PVConstraint *PVC, ConstAtom *NewA, Constraints &CS);
  CVarSet addAtomAll(CVarSet CVS, ConstAtom *PtrTyp, Constraints &CS);
  CVarSet getWildPVConstraint();
  CVarSet PVConstraintFromType(QualType TypE);

  CVarSet getAllSubExprConstraintVars(std::vector<Expr *> &Exprs);
  CVarSet getBaseVarPVConstraint(DeclRefExpr *Decl);

  PVConstraint *getRewritablePVConstraint(Expr *E);
};

#endif // _CONSTRAINTRESOLVER_H
