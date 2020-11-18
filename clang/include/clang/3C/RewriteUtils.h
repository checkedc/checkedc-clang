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

#include "ProgramInfo.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace clang;

class DeclReplacement {
public:
  virtual Decl *getDecl() const = 0;

  DeclStmt *getStatement() const { return Statement; }

  std::string getReplacement() const { return Replacement; }

  virtual SourceRange getSourceRange(SourceManager &SM) const {
    return getDecl()->getSourceRange();
  }

  // Discriminator for LLVM-style RTTI (dyn_cast<> et al.)
  enum DRKind { DRK_VarDecl, DRK_ParmVarDecl, DRK_FunctionDecl, DRK_FieldDecl };

  DRKind getKind() const { return Kind; }

  virtual ~DeclReplacement() {}

protected:
  explicit DeclReplacement(DeclStmt *S, std::string R, DRKind K)
      : Statement(S), Replacement(R), Kind(K) {}

  // The Stmt, if it exists (may be nullptr).
  DeclStmt *Statement;

  // The string to replace the declaration with.
  std::string Replacement;

private:
  const DRKind Kind;
};

template <typename DeclT, DeclReplacement::DRKind K>
class DeclReplacementTempl : public DeclReplacement {
public:
  explicit DeclReplacementTempl(DeclT *D, DeclStmt *DS, std::string R)
      : DeclReplacement(DS, R, K), Decl(D) {}

  DeclT *getDecl() const override { return Decl; }

  static bool classof(const DeclReplacement *S) { return S->getKind() == K; }

protected:
  DeclT *Decl;
};

typedef DeclReplacementTempl<VarDecl, DeclReplacement::DRK_VarDecl>
    VarDeclReplacement;
typedef DeclReplacementTempl<ParmVarDecl, DeclReplacement::DRK_ParmVarDecl>
    ParmVarDeclReplacement;
typedef DeclReplacementTempl<FieldDecl, DeclReplacement::DRK_FieldDecl>
    FieldDeclReplacement;

class FunctionDeclReplacement
    : public DeclReplacementTempl<FunctionDecl,
                                  DeclReplacement::DRK_FunctionDecl> {
public:
  explicit FunctionDeclReplacement(FunctionDecl *D, std::string R, bool Return,
                                   bool Params)
      : DeclReplacementTempl(D, nullptr, R), RewriteReturn(Return),
        RewriteParams(Params) {
    assert("Doesn't make sense to rewrite nothing!" &&
           (RewriteReturn || RewriteParams));
  }

  SourceRange getSourceRange(SourceManager &SM) const override {
    TypeSourceInfo *TSInfo = Decl->getTypeSourceInfo();
    if (!TSInfo)
      return SourceRange(Decl->getBeginLoc(),
                         getFunctionDeclarationEnd(Decl, SM));
    FunctionTypeLoc TypeLoc =
        getBaseTypeLoc(TSInfo->getTypeLoc()).getAs<clang::FunctionTypeLoc>();

    assert("FunctionDecl doesn't have function type?" && !TypeLoc.isNull());

    // Function pointer are funky, and require special handling to rewrite the
    // return type.
    if (Decl->getReturnType()->isFunctionPointerType()) {
      if (RewriteParams && RewriteReturn) {
        auto T =
            getBaseTypeLoc(TypeLoc.getReturnLoc()).getAs<FunctionTypeLoc>();
        if (!T.isNull())
          return SourceRange(Decl->getBeginLoc(), T.getRParenLoc());
      }
      // Fall through to standard handling when only rewriting param decls
    }

    // If rewriting the return, then the range starts at the begining of the
    // decl. Otherwise, skip to the left parenthesis of parameters.
    SourceLocation Begin =
        RewriteReturn ? Decl->getBeginLoc() : TypeLoc.getLParenLoc();

    // If rewriting Parameters, stop at the right parenthesis of the parameters.
    // Otherwise, stop after the return type.
    // Note: getFunctionDeclarationEnd is used instead of getRParenLoc so that
    // itypes are deleted correctly when --remove-itypes is used.
    SourceLocation End = RewriteParams
                             ? getFunctionDeclarationEnd(Decl, SM)
                             : Decl->getReturnTypeSourceRange().getEnd();

    assert("Invalid FunctionDeclReplacement SourceRange!" && Begin.isValid() &&
           End.isValid());

    return SourceRange(Begin, End);
  }

private:
  // This determines if the full declaration or the return will be replaced.
  bool RewriteReturn;

  bool RewriteParams;
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
  DComp(SourceManager &S) : SM(S) {}

  bool operator()(DeclReplacement *Lhs, DeclReplacement *Rhs) const;

private:
  SourceManager &SM;

  SourceRange getReplacementSourceRange(DeclReplacement *D) const;
  SourceLocation getDeclBegin(DeclReplacement *D) const;
};

typedef std::set<DeclReplacement *, DComp> RSet;

// This class is used to figure out which global variables are part of
// multi-variable declarations. For local variables, all variables in a single
// multi declaration are grouped together in a DeclStmt object. This is not the
// case for global variables, so this class is required to correctly group
// global variable declarations. Declarations in the same multi-declarations
// have the same beginning source locations, so it is used to group variables.
class GlobalVariableGroups {
public:
  GlobalVariableGroups(SourceManager &SourceMgr) : SM(SourceMgr) {}
  void addGlobalDecl(Decl *VD, std::vector<Decl *> *VDVec = nullptr);

  std::vector<Decl *> &getVarsOnSameLine(Decl *VD);

  virtual ~GlobalVariableGroups();

private:
  SourceManager &SM;
  std::map<Decl *, std::vector<Decl *> *> GlobVarGroups;
};

// Class that handles rewriting bounds information for all the
// detected array variables.
class ArrayBoundsRewriter {
public:
  ArrayBoundsRewriter(ASTContext *C, ProgramInfo &I) : Context(C), Info(I) {}
  // Get the string representation of the bounds for the given variable.
  std::string getBoundsString(PVConstraint *PV, Decl *D, bool Isitype = false);
  // Check if the constraint variable has newly created bounds string.
  bool hasNewBoundsString(PVConstraint *PV, Decl *D, bool Isitype = false);

private:
  ASTContext *Context;
  ProgramInfo &Info;
};

class RewriteConsumer : public ASTConsumer {
public:
  explicit RewriteConsumer(ProgramInfo &I, std::string &OPostfix)
      : Info(I), OutputPostfix(OPostfix) {}

  virtual void HandleTranslationUnit(ASTContext &Context);

private:
  ProgramInfo &Info;
  static std::map<std::string, std::string> ModifiedFuncSignatures;
  std::string &OutputPostfix;

  // A single header file can be included in multiple translations units. This
  // set ensures that the diagnostics for a header file are not emitted each
  // time a translation unit containing the header is vistied.
  static std::set<PersistentSourceLoc> EmittedDiagnostics;

  void emitRootCauseDiagnostics(ASTContext &Context);
};

bool canRewrite(Rewriter &R, SourceRange &SR);

#endif //_REWRITEUTILS_H
