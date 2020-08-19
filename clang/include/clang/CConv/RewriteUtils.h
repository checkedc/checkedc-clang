//=--RewriteUtils.h-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains functions and classes that deal with
// rewriting the source file after converting to CheckedC format.
//===----------------------------------------------------------------------===//

#ifndef _REWRITEUTILS_H
#define _REWRITEUTILS_H

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "ProgramInfo.h"

using namespace clang;

class DeclReplacement {
public:
  virtual Decl *getDecl() const = 0;

  DeclStmt *getStatement() const { return Statement; }

  std::string getReplacement() const { return Replacement; }

  virtual SourceRange getSourceRange(SourceManager &SR) const {
    return getDecl()->getSourceRange();
  }

  virtual ~DeclReplacement() {}
protected:
  explicit DeclReplacement(DeclStmt *S, std::string R)
      : Statement(S), Replacement(R) {}

  // The Stmt, if it exists (may be nullptr).
  DeclStmt *Statement;

  // The string to replace the declaration with.
  std::string Replacement;
};

template<typename DeclT>
class DeclReplacementTempl : public DeclReplacement {
public:
  DeclT *getDecl() const override {
    return Decl;
  }

  explicit DeclReplacementTempl(DeclT *D, DeclStmt *DS, std::string R)
      : DeclReplacement(DS, R), Decl(D) {}

protected:
  DeclT *Decl;
};

typedef DeclReplacementTempl<VarDecl> VarDeclReplacement;
typedef DeclReplacementTempl<ParmVarDecl> ParmVarDeclReplacement;
typedef DeclReplacementTempl<FieldDecl> FieldDeclReplacement;

class FunctionDeclReplacement : public DeclReplacementTempl<FunctionDecl> {
public:
  explicit FunctionDeclReplacement(FunctionDecl *D, std::string R, bool Full)
      : DeclReplacementTempl<FunctionDecl>(D, nullptr, R),
        FullDecl(Full) {}

  SourceRange getSourceRange(SourceManager &SM) const override {
    if (FullDecl) {
      SourceRange Range = Decl->getSourceRange();
      Range.setEnd(getFunctionDeclarationEnd(Decl, SM));
      return Range;
    } else
      return Decl->getReturnTypeSourceRange();
  }

  bool isFullDecl() const {
    return FullDecl;
  }

private:
  // This determines if the full declaration or the return will be replaced.
  bool FullDecl;
};

// Compare two DeclReplacement values. The algorithm for comparing them relates
// their source positions. If two DeclReplacement values refer to overlapping
// source positions, then they are the same. Otherwise, they are ordered
// by their placement in the input file.
//
// There are two special cases: Function declarations, and DeclStmts. In turn:
//
//  - Function declarations might either be a DeclReplacement describing the
//    entire declaration, i.e. replacing "int *foo(void)"
//    with "int *foo(void) : itype(_Ptr<int>)". Or, it might describe just
//    replacing only the return type, i.e. "_Ptr<int> foo(void)". This is
//    discriminated against with the 'fullDecl' field of the DeclReplacement
//    type and the comparison function first checks if the operands are
//    FunctionDecls and if the 'fullDecl' field is set.
//  - A DeclStmt of mupltiple Decls, i.e. 'int *a = 0, *b = 0'. In this case,
//    we want the DeclReplacement to refer only to the specific sub-region that
//    would be replaced, i.e. '*a = 0' and '*b = 0'. To do that, we traverse
//    the Decls contained in a DeclStmt and figure out what the appropriate
//    source locations are to describe the positions of the independent
//    declarations.
class DComp {
public:
  DComp(SourceManager &S) : SM(S) { }

  bool operator()(DeclReplacement *Lhs, DeclReplacement *Rhs) const;

private:
  SourceManager &SM;

  SourceRange getReplacementSourceRange(DeclReplacement *D) const;
  SourceLocation getDeclBegin(DeclReplacement *D) const;
};

typedef std::set<DeclReplacement *, DComp> RSet;

// Class that maintains global variables according to the line numbers
// this groups global variables according to the line numbers in source files.
// All global variables that belong to the same file and are on the same line
// will be in the same group.
// e.g., int *a,*b; // both will be in same group
// where as
// int *c;
// int *d
// will be in different groups.

class GlobalVariableGroups {
public:
  GlobalVariableGroups(SourceManager &SourceMgr) : SM(SourceMgr) { }
  void addGlobalDecl(Decl *VD, std::set<Decl *> *VDSet = nullptr);

  std::set<Decl *> &getVarsOnSameLine(Decl *VD);

  virtual ~GlobalVariableGroups();

private:
  SourceManager &SM;
  std::map<Decl *, std::set<Decl *>*> GlobVarGroups;
};

// Class that handles rewriting bounds information for all the
// detected array variables.
class ArrayBoundsRewriter {
public:
  ArrayBoundsRewriter(ASTContext *C, ProgramInfo &I): Context(C), Info(I) {}
  // Get the string representation of the bounds for the given variable.
  std::string getBoundsString(PVConstraint *PV, Decl *D, bool Isitype = false);
private:
  ASTContext *Context;
  ProgramInfo &Info;
};

class RewriteConsumer : public ASTConsumer {
public:
  explicit RewriteConsumer(ProgramInfo &I, std::string &OPostfix) :
                           Info(I), OutputPostfix(OPostfix) {}

  virtual void HandleTranslationUnit(ASTContext &Context);

private:
  ProgramInfo &Info;
  static std::map<std::string, std::string> ModifiedFuncSignatures;
  std::string &OutputPostfix;
};

bool canRewrite(Rewriter &R, SourceRange &SR);

#endif //_REWRITEUTILS_H
