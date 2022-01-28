//=--DeclRewriter.h-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This file contains the DeclRewriter class which is used to rewrite variable
// declarations in a program using the checked pointers types solved for by the
// the conversion tool.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_DECLREWRITER_H
#define LLVM_CLANG_3C_DECLREWRITER_H

#include "clang/3C/ConstraintBuilder.h"
#include "clang/3C/RewriteUtils.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace llvm;
using namespace clang;

class DeclRewriter {
public:
  DeclRewriter(Rewriter &R, ProgramInfo &Info, ASTContext &A)
      : R(R), Info(Info), A(A) {}

  // The publicly accessible interface for performing declaration rewriting.
  // All declarations for variables with checked types in the variable map of
  // Info parameter are rewritten.
  static void rewriteDecls(ASTContext &Context, ProgramInfo &Info, Rewriter &R);

  static RewrittenDecl buildItypeDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                                      std::string UseName, ProgramInfo &Info,
                                      ArrayBoundsRewriter &ABR,
                                      bool GenerateSDecls, bool SDeclChecked);

  static RewrittenDecl
  buildCheckedDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                   std::string UseName, ProgramInfo &Info,
                   ArrayBoundsRewriter &ABR, bool GenerateSDecls);

private:
  Rewriter &R;
  ProgramInfo &Info;
  ASTContext &A;

  // List of TagDecls that were split from multi-decls and should be moved out
  // of an enclosing RecordDecl to avoid a compiler warning. Filled during
  // multi-decl rewriting and processed by denestTagDecls.
  std::vector<TagDecl *> TagDeclsToDenest;

  // Visit each Decl in ToRewrite and apply the appropriate pointer type
  // to that Decl. ToRewrite is the set of all declarations to rewrite.
  void rewrite(RSet &ToRewrite);

  void rewriteMultiDecl(MultiDeclInfo &MDI, RSet &ToRewrite);
  void doDeclRewrite(SourceRange &SR, DeclReplacement *N);
  void rewriteFunctionDecl(FunctionDeclReplacement *N);
  // Emit supplementary declarations _after_ the token that begins at Loc.
  // Inserts a newline before the first supplementary declaration but not after
  // the last supplementary declaration. This is suitable if Loc is expected to
  // be the last token on a line or if rewriteMultiDecl will insert a newline
  // after the supplementary declarations later.
  void emitSupplementaryDeclarations(const std::vector<std::string> &SDecls,
                                     SourceLocation Loc);
  SourceLocation getNextCommaOrSemicolon(SourceLocation L);
  void denestTagDecls();
};

// Visits function declarations and adds entries with their new rewritten
// declaration to the RSet RewriteThese.
class FunctionDeclBuilder : public RecursiveASTVisitor<FunctionDeclBuilder> {
public:
  explicit FunctionDeclBuilder(ASTContext *C, ProgramInfo &I, RSet &DR,
                               ArrayBoundsRewriter &ArrRewriter)
      : Context(C), Info(I), RewriteThese(DR), ABRewriter(ArrRewriter),
        VisitedSet() {}

  bool VisitFunctionDecl(FunctionDecl *);
  bool isFunctionVisited(std::string FuncName);

protected:
  ASTContext *Context;
  ProgramInfo &Info;
  RSet &RewriteThese;
  ArrayBoundsRewriter &ABRewriter;

  // Set containing the names of all functions visited in the AST traversal.
  // Used to ensure the new signature is only computed once for each function.
  std::set<std::string> VisitedSet;

  // Get existing itype string from constraint variables.
  std::string getExistingIType(ConstraintVariable *DeclC);

  virtual RewrittenDecl
  buildDeclVar(const FVComponentVariable *CV, DeclaratorDecl *Decl,
               std::string UseName, bool &RewriteGen, bool &RewriteParm,
               bool &RewriteRet, bool StaticFunc, bool GenerateSDecls);

  RewrittenDecl
  buildCheckedDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                   std::string UseName, bool &RewriteParm, bool &RewriteRet,
                   bool GenerateSDecls);

  RewrittenDecl buildItypeDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                               std::string UseName, bool &RewriteParm,
                               bool &RewriteRet, bool GenerateSDecls,
                               bool SDeclChecked);

  bool inParamMultiDecl(const ParmVarDecl *PVD);
};
#endif // LLVM_CLANG_3C_DECLREWRITER_H
