//=--CheckedRegions.h---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains functions and classes that deal with
// adding the _Checked and _Unchecked annotations to code
// when enabled with the -addcr flag
//===----------------------------------------------------------------------===//

#ifndef _CHECKEDREGIONS_H
#define _CHECKEDREGIONS_H

#include <utility>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "ProgramInfo.h"

typedef enum {
  IS_UNCHECKED,
  IS_CHECKED,
  IS_CONTAINED,
} AnnotationNeeded;

class CheckedRegionAdder
    : public clang::RecursiveASTVisitor<CheckedRegionAdder> {
public:
  explicit CheckedRegionAdder(
      clang::ASTContext *_C, clang::Rewriter &_R,
      std::map<llvm::FoldingSetNodeID, AnnotationNeeded> &M)
      : Context(_C), Writer(_R), Map(M) {}

  bool VisitCompoundStmt(clang::CompoundStmt *S);
  bool VisitCallExpr(clang::CallExpr *C);

private:
  std::pair<const clang::CompoundStmt *, int>
  findParentCompound(const clang::ast_type_traits::DynTypedNode &N, int);
  bool isParentChecked(const clang::ast_type_traits::DynTypedNode &N);
  bool isWrittenChecked(const clang::CompoundStmt *);
  bool isFunctionBody(clang::CompoundStmt *S);
  clang::ASTContext *Context;
  clang::Rewriter &Writer;
  std::map<llvm::FoldingSetNodeID, AnnotationNeeded> &Map;
};

class CheckedRegionFinder
    : public clang::RecursiveASTVisitor<CheckedRegionFinder> {
public:
  explicit CheckedRegionFinder(
      clang::ASTContext *_C, clang::Rewriter &_R, ProgramInfo &_I,
      std::set<llvm::FoldingSetNodeID> &S,
      std::map<llvm::FoldingSetNodeID, AnnotationNeeded> &M, bool EmitWarnings)
      : Context(_C), Writer(_R), Info(_I), Seen(S), Map(M),
        EmitWarnings(EmitWarnings) {}
  bool Wild = false;

  bool VisitForStmt(clang::ForStmt *S);
  bool VisitSwitchStmt(clang::SwitchStmt *S);
  bool VisitIfStmt(clang::IfStmt *S);
  bool VisitWhileStmt(clang::WhileStmt *S);
  bool VisitDoStmt(clang::DoStmt *S);
  bool VisitCompoundStmt(clang::CompoundStmt *S);
  bool VisitStmtExpr(clang::StmtExpr *SE);
  bool VisitCStyleCastExpr(clang::CStyleCastExpr *E);
  bool VisitCallExpr(clang::CallExpr *C);
  bool VisitVarDecl(clang::VarDecl *VD);
  bool VisitParmVarDecl(clang::ParmVarDecl *PVD);
  bool VisitMemberExpr(clang::MemberExpr *E);
  bool VisitDeclRefExpr(clang::DeclRefExpr *);

private:
  void handleChildren(const clang::Stmt::child_range &Stmts);
  void markChecked(clang::CompoundStmt *S, int LocalWild);
  bool isInStatementPosition(clang::CallExpr *C);
  bool hasUncheckedParameters(clang::CompoundStmt *S);
  bool containsUncheckedPtr(clang::QualType Qt);
  bool containsUncheckedPtrAcc(clang::QualType Qt, std::set<std::string> &Seen);
  bool isUncheckedStruct(clang::QualType Qt, std::set<std::string> &Seen);
  void emitCauseDiagnostic(PersistentSourceLoc *);
  bool isWild(CVarOption CVar);

  clang::ASTContext *Context;
  clang::Rewriter &Writer;
  ProgramInfo &Info;
  std::set<llvm::FoldingSetNodeID> &Seen;
  std::map<llvm::FoldingSetNodeID, AnnotationNeeded> &Map;
  std::set<PersistentSourceLoc *> Emitted;
  bool EmitWarnings;
};

#endif //_CHECKEDREGIONS_H
