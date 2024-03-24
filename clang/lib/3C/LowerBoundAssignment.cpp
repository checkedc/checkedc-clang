//=--LowerBoundAssignment.cpp-------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of methods in LowerBoundAssignment.h
//===----------------------------------------------------------------------===//

#include "clang/3C/ProgramInfo.h"
#include "clang/AST/ASTContext.h"

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/3C/LowerBoundAssignment.h"
#include "clang/3C/RewriteUtils.h"

using namespace llvm;
using namespace clang;

// Visitor to collect all the variables and structure member access that are
// used during the life-time of the visitor.
class CollectDeclsVisitor : public RecursiveASTVisitor<CollectDeclsVisitor> {
public:
  explicit CollectDeclsVisitor() : ObservedDecls(), ObservedStructAccesses() {}

  virtual ~CollectDeclsVisitor() {}

  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    if (auto *VD = dyn_cast_or_null<VarDecl>(DRE->getDecl()))
      ObservedDecls.insert(VD);
    return true;
  }

  // For `a->b` we need to get `a->b` rather than just `b`. This way assignment
  // from a field in one instance of a structure to the same field in another
  // instance is not treated as pointer arithmetic.
  bool VisitMemberExpr(MemberExpr *ME) {
    // TODO: Is this cast legit? `getMemberDecl()` returns a `ValueDecl`, but I
    //       think it can only be a `FieldDecl` for structs in C.
    auto *FD = cast<FieldDecl>(ME->getMemberDecl());

    CollectDeclsVisitor MEVis;
    MEVis.TraverseStmt(ME->getBase());
    // Field access through variable.
    for (auto *D : MEVis.getObservedDecls()) {
      std::vector<FieldDecl *> SingletonAccessList({FD});
      ObservedStructAccesses.insert(std::make_pair(D, SingletonAccessList));
    }
    // Field access through other structure fields.
    for (StructAccess SA : MEVis.getObservedStructAccesses()) {
      SA.second.push_back(FD);
      ObservedStructAccesses.insert(SA);
    }
    return false;
  }

  bool VisitCallExpr(CallExpr *CE) {
    // Stop the visitor when we hit a CallExpr. This stops us from treating a
    // function call like `a = foo(a);` the same as `a = a + 1`.
    return false;
  }

  const std::set<VarDecl *> &getObservedDecls() { return ObservedDecls; }

  // A structure field access is represented
  typedef std::pair<VarDecl *, std::vector<FieldDecl *>> StructAccess;
  const std::set<StructAccess> &getObservedStructAccesses() {
    return ObservedStructAccesses;
  }

private:
  // Contains all VarDecls seen by this visitor
  std::set<VarDecl *> ObservedDecls;

  // Contains the source representation of all record access (MemberExpression)
  // seen by this visitor.
  std::set<StructAccess> ObservedStructAccesses;
};

bool isLowerBoundAssignment(Expr *LHS, Expr *RHS) {
  CollectDeclsVisitor LVarVis;
  LVarVis.TraverseStmt(LHS);

  CollectDeclsVisitor RVarVis;
  RVarVis.TraverseStmt(RHS);

  std::set<VarDecl *> CommonVars;
  std::set<CollectDeclsVisitor::StructAccess> CommonStVars;
  findIntersection(LVarVis.getObservedDecls(), RVarVis.getObservedDecls(),
                   CommonVars);
  findIntersection(LVarVis.getObservedStructAccesses(),
                   RVarVis.getObservedStructAccesses(), CommonStVars);

  // If CommonVars is empty, then the same pointer does not appears on the LHS
  // and RHS of the assignment. We say that the assignment is a lower bound
  // update.
  return CommonVars.empty() && CommonStVars.empty();
}

bool
LowerBoundAssignmentVisitor::VisitBinaryOperator(BinaryOperator *O) {
  if (O->getOpcode() == clang::BO_Assign &&
    isLowerBoundAssignment(O->getLHS(), O->getRHS()))
    visitLowerBoundAssignment(O->getLHS(), O->getRHS());
  return true;
}

void LowerBoundAssignmentUpdater::visitLowerBoundAssignment(Expr *LHS,
                                                            Expr *RHS) {
  CVarSet LHSCVs = CR.getExprConstraintVarsSet(LHS);
  // It is possible for multiple ConstraintVariables to exist on the LHS
  // of an assignment expression; e.g., `*(0 ? a : b) = 0`. If this
  // happens, and one of those variables needed range bounds, then the
  // following rewriting is not correct. I believe that it can only happen
  // when the LHS is a pointer dereference or struct field access.
  // Structure fields and inner pointer levels can never have range bounds
  // so this case currently is not possible.
  assert(LHSCVs.size() == 1 || llvm::count_if(LHSCVs, [this](
    ConstraintVariable *CV) { return ABInfo.needsFreshLowerBound(CV); }) == 0);
  for (ConstraintVariable *CV: LHSCVs) {
    if (ABInfo.needsFreshLowerBound(CV)) {
      BoundsKey LBKey = ABInfo.getBounds(
        CV->getBoundsKey())->getLowerBoundKey();
      assert(
        "Should not be rewriting assignments for pointer without lower bound!" &&
        LBKey != 0);
      std::string LBName = ABInfo.getProgramVar(LBKey)->getVarName();
      rewriteSourceRange(R, LHS->getSourceRange(), LBName);
      R.InsertTextAfter(
        getLocationAfterToken(RHS->getEndLoc(), R.getSourceMgr(),
                              R.getLangOpts()),
        ", " + CV->getName() + " = " + LBName);
    }
  }
}

void LowerBoundAssignmentFinder::visitLowerBoundAssignment(Expr *LHS,
                                                           Expr *RHS) {
  SourceLocation RHSEnd =
    getLocationAfterToken(RHS->getEndLoc(), C->getSourceManager(),
                          C->getLangOpts());
  SourceLocation LHSLoc = LHS->getExprLoc();
  if (!(LHSLoc.isValid() && Rewriter::isRewritable(LHSLoc)) ||
      !(RHSEnd.isValid() && Rewriter::isRewritable(RHSEnd))) {
    CVarSet LHSCVs = CR.getExprConstraintVarsSet(LHS);
    for (auto *CV: LHSCVs)
      if (CV->hasBoundsKey())
        ABInfo.markIneligibleForFreshLowerBound(CV->getBoundsKey());
  }
}