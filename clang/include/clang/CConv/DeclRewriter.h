//=--DeclRewriter.h-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_CCONV_DECLREWRITER_H
#define LLVM_CLANG_LIB_CCONV_DECLREWRITER_H

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/CConv/RewriteUtils.h"

using namespace llvm;
using namespace clang;

class DeclRewriter {
public:
  DeclRewriter(Rewriter &R, ASTContext &A, GlobalVariableGroups &GP)
      : R(R), A(A), GP(GP), Skip(DComp(A.getSourceManager())) {}

  // Visit each Decl in toRewrite and apply the appropriate pointer type
  // to that Decl. The state of the rewrite is contained within R, which
  // is both input and output. R is initialized to point to the 'main'
  // source file for this transformation. toRewrite contains the set of
  // declarations to rewrite. S is passed for source-level information
  // about the current compilation unit.
  void rewrite(RSet &ToRewrite, std::set<FileID> &TouchedFiles);

private:
  Rewriter &R;
  ASTContext &A;
  GlobalVariableGroups &GP;
  // Skip indicates some rewrites that
  // we should skip because we already applied them, for example, as part
  // of turning a single line declaration into a multi-line declaration.
  RSet Skip;

  void rewrite(VarDecl *VD, std::string SRewrite, Stmt *WhereStmt,
               const DAndReplace &N, RSet &ToRewrite);
  void rewrite(ParmVarDecl *PV, std::string SRewrite);
  unsigned int getParameterIndex(ParmVarDecl *PV, FunctionDecl *FD);
  SourceLocation deleteAllDeclarationsOnLine(VarDecl *VD, DeclStmt *Stmt);
  void getDeclsOnSameLine(VarDecl *VD, DeclStmt *Stmt, std::set<Decl *> &Decls);
  bool isSingleDeclaration(VarDecl *VD, DeclStmt *Stmt);
  bool areDeclarationsOnSameLine(VarDecl *VD1, DeclStmt *Stmt1, VarDecl *VD2,
                                 DeclStmt *Stmt2);
};

#endif //LLVM_CLANG_LIB_CCONV_DECLREWRITER_H
