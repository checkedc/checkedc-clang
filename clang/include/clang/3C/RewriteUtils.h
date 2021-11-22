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

#ifndef LLVM_CLANG_3C_REWRITEUTILS_H
#define LLVM_CLANG_3C_REWRITEUTILS_H

#include "clang/3C/ProgramInfo.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace clang;

class DeclReplacement {
public:
  virtual Decl *getDecl() const = 0;

  std::string getReplacement() const { return Replacement; }

  virtual SourceRange getSourceRange(SourceManager &SM) const;

  // Discriminator for LLVM-style RTTI (dyn_cast<> et al.).
  enum DRKind {
    DRK_MultiDeclMember,
    DRK_FunctionDecl,
  };

  DRKind getKind() const { return Kind; }

  virtual ~DeclReplacement() {}

protected:
  explicit DeclReplacement(std::string R, DRKind K) : Replacement(R), Kind(K) {}

  // The string to replace the declaration with.
  std::string Replacement;

private:
  const DRKind Kind;
};

template <typename DeclT, DeclReplacement::DRKind K>
class DeclReplacementTempl : public DeclReplacement {
public:
  explicit DeclReplacementTempl(DeclT *D, std::string R)
      : DeclReplacement(R, K), Decl(D) {}

  DeclT *getDecl() const override { return Decl; }

  static bool classof(const DeclReplacement *S) { return S->getKind() == K; }

protected:
  DeclT *Decl;
};

typedef DeclReplacementTempl<NamedDecl, DeclReplacement::DRK_MultiDeclMember>
    MultiDeclMemberReplacement;

class FunctionDeclReplacement
    : public DeclReplacementTempl<FunctionDecl,
                                  DeclReplacement::DRK_FunctionDecl> {
public:
  explicit FunctionDeclReplacement(FunctionDecl *D, std::string R, bool Return,
                                   bool Params, bool Generic = false)
      : DeclReplacementTempl(D, R), RewriteGeneric(Generic),
        RewriteReturn(Return), RewriteParams(Params) {
    assert("Doesn't make sense to rewrite nothing!" &&
           (RewriteGeneric || RewriteReturn || RewriteParams));
  }

  SourceRange getSourceRange(SourceManager &SM) const override;

private:
  // This determines if the full declaration or the return will be replaced.
  bool RewriteGeneric;
  bool RewriteReturn;
  bool RewriteParams;

  SourceLocation getDeclBegin(SourceManager &SM) const;
  SourceLocation getReturnBegin(SourceManager &SM) const;
  SourceLocation getParamBegin(SourceManager &SM) const;
  SourceLocation getReturnEnd(SourceManager &SM) const;
  SourceLocation getDeclEnd(SourceManager &SM) const;
};

typedef std::map<Decl *, DeclReplacement *> RSet;

// Generate a string for the declaration based on the given PVConstraint.
// Includes the storage qualifier, type, name, and bounds string (as
// applicable), or generates an itype declaration if required due to
// ItypesForExtern. Does not include a trailing semicolon or an initializer, so
// it can be used in combination with getDeclSourceRangeWithAnnotations with
// IncludeInitializer = false to preserve an existing initializer.
std::string mkStringForPVDecl(MultiDeclMemberDecl *MMD, PVConstraint *PVC,
                              ProgramInfo &Info);

// Generate a string like mkStringForPVDecl, but for a declaration whose type is
// known not to have changed (except possibly for a base type rename) and that
// may not have a PVConstraint if the type is not a pointer or array type.
//
// For similar reasons as in the comment in DeclRewriter::buildItypeDecl, this
// will get the string from Clang instead of mkString if the base type hasn't
// been renamed (hence the need to assume the rest of the type has not changed).
// Yet another possible approach would be to combine the new base type name with
// the original source for the rest of the declaration, but that may run into
// problems with macros and the like, so we might still need some fallback. For
// now, we don't implement this "original source" approach.
std::string mkStringForDeclWithUnchangedType(MultiDeclMemberDecl *D,
                                             ProgramInfo &Info);

// Class that handles rewriting bounds information for all the
// detected array variables.
class ArrayBoundsRewriter {
public:
  ArrayBoundsRewriter(ProgramInfo &I) : Info(I) {}
  // Get the string representation of the bounds for the given variable.
  std::string getBoundsString(const PVConstraint *PV, Decl *D,
                              bool Isitype = false);

  // Check if the constraint variable has newly created bounds string.
  bool hasNewBoundsString(const PVConstraint *PV, Decl *D,
                          bool Isitype = false);

private:
  ProgramInfo &Info;
};

class RewriteConsumer : public ASTConsumer {
public:
  explicit RewriteConsumer(ProgramInfo &I) : Info(I) {}

  void HandleTranslationUnit(ASTContext &Context) override;

private:
  ProgramInfo &Info;
  static std::map<std::string, std::string> ModifiedFuncSignatures;

  // A single header file can be included in multiple translations units. This
  // set ensures that the diagnostics for a header file are not emitted each
  // time a translation unit containing the header is vistied.
  static std::set<PersistentSourceLoc> EmittedDiagnostics;

  void emitRootCauseDiagnostics(ASTContext &Context);

  // Hack to avoid printing the main file to stdout multiple times in the edge
  // case of a compilation database containing multiple translation units for
  // the main file
  // (https://github.com/correctcomputation/checkedc-clang/issues/374#issuecomment-893612654).
  bool StdoutModeEmittedMainFile = false;
};

bool canRewrite(Rewriter &R, const SourceRange &SR);

bool canRewrite(clang::Expr &D, ASTContext &Context);

// Rewrites the given source range with fallbacks for when the SourceRange is
// inside a macro. This should be preferred to direct calls to ReplaceText
// because this function will automatically expand macros where it needs to and
// emits an error if it cannot rewrite even after expansion. If there is a
// rewriting that is known to fail in circumstances where we want to maintain
// a zero exit code, ErrFail can be set to false. This downgrades rewrite
// failures to a warning.
void rewriteSourceRange(Rewriter &R, const CharSourceRange &Range,
                        const std::string &NewText, bool ErrFail = true);

void rewriteSourceRange(Rewriter &R, const SourceRange &Range,
                        const std::string &NewText, bool ErrFail = true);

#endif // LLVM_CLANG_3C_REWRITEUTILS_H
