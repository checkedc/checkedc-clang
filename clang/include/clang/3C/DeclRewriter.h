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

#ifndef _DECLREWRITER_H
#define _DECLREWRITER_H

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
  DeclRewriter(Rewriter &R, ASTContext &A, GlobalVariableGroups &GP)
      : R(R), A(A), GP(GP),
        VisitedMultiDeclMembers(DComp(A.getSourceManager())) {}

  // The publicly accessible interface for performing declaration rewriting.
  // All declarations for variables with checked types in the variable map of
  // Info parameter are rewritten.
  static void rewriteDecls(ASTContext &Context, ProgramInfo &Info, Rewriter &R);

private:
  Rewriter &R;
  ASTContext &A;
  GlobalVariableGroups &GP;

  // This set contains declarations that have already been rewritten as part of
  // a prior declaration that was in the same multi-declaration. It is checked
  // before rewriting in order to avoid rewriting a declaration more than once.
  // It is not used with individual declarations outside of multi-declarations
  // because these declarations are seen exactly once, rather than every time a
  // declaration in the containing multi-decl is visited.
  RSet VisitedMultiDeclMembers;

  // TODO: I don't like having this be static, but it needs to be static in
  //       order to pass information between different translation units. A
  //       new instance of this class (and the RewriteConsumer class) is created
  //       for each translation unit.
  static std::map<std::string, std::string> NewFuncSig;

  // Visit each Decl in ToRewrite and apply the appropriate pointer type
  // to that Decl. ToRewrite is the set of all declarations to rewrite.
  void rewrite(RSet &ToRewrite);

  // Rewrite a specific variable declaration using the replacement string in the
  // DAndReplace structure. Each of these functions is specialized to handling
  // one subclass of declarations.
  void rewriteParmVarDecl(ParmVarDeclReplacement *N);

  template <typename DRType>
  void rewriteFieldOrVarDecl(DRType *N, RSet &ToRewrite);
  void rewriteMultiDecl(DeclReplacement *N, RSet &ToRewrite);
  void rewriteSingleDecl(DeclReplacement *N, RSet &ToRewrite);
  void doDeclRewrite(SourceRange &SR, DeclReplacement *N);

  void rewriteFunctionDecl(FunctionDeclReplacement *N);
  void getDeclsOnSameLine(DeclReplacement *N, std::vector<Decl *> &Decls);
  bool isSingleDeclaration(DeclReplacement *N);
  bool areDeclarationsOnSameLine(DeclReplacement *N1, DeclReplacement *N2);
  SourceRange getNextCommaOrSemicolon(SourceLocation L);
};

// Visits function declarations and adds entries with their new rewritten
// declaration to the RSet RewriteThese.
class FunctionDeclBuilder : public RecursiveASTVisitor<FunctionDeclBuilder> {
public:
  explicit FunctionDeclBuilder(ASTContext *C, ProgramInfo &I, RSet &DR,
                               std::map<std::string, std::string> &NewFuncSig,
                               ArrayBoundsRewriter &ArrRewriter)
      : Context(C), Info(I), RewriteThese(DR), ABRewriter(ArrRewriter),
        VisitedSet(), ModifiedFuncSignatures(NewFuncSig) {}

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

  // This is a map from functions (the string representation of their names) to
  // their function signature in the rewritten program.
  std::map<std::string, std::string> &ModifiedFuncSignatures;

  // Get existing itype string from constraint variables.
  std::string getExistingIType(ConstraintVariable *DeclC);
  virtual void buildDeclVar(PVConstraint *Defn, DeclaratorDecl *Decl,
                            std::string &Type, std::string &IType,
                            bool &RewriteParm, bool &RewriteRet);
  void buildCheckedDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                        std::string &Type, std::string &IType,
                        bool &RewriteParm, bool &RewriteRet);
  void buildItypeDecl(PVConstraint *Defn, DeclaratorDecl *Decl,
                      std::string &Type, std::string &IType, bool &RewriteParm,
                      bool &RewriteRet);
};

class FieldFinder : public RecursiveASTVisitor<FieldFinder> {
public:
  FieldFinder(GlobalVariableGroups &GVG) : GVG(GVG) {}

  bool VisitFieldDecl(FieldDecl *FD);

  static void gatherSameLineFields(GlobalVariableGroups &GVG, Decl *D);

private:
  GlobalVariableGroups &GVG;
};
#endif //_DECLREWRITER_H
