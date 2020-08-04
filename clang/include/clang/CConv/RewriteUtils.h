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

// A Declaration, optional DeclStmt, and a replacement string
// for that Declaration.
struct DAndReplace
{
    Decl        *Declaration; // The declaration to replace.
    DeclStmt    *Statement;   // The Stmt, if it exists.
    std::string Replacement;  // The string to replace the declaration with.
    bool        FullDecl;     // If the declaration is a function, true if
    // Replace the entire declaration or just the
    // return declaration.
    DAndReplace() : Declaration(nullptr),
                    Statement(nullptr),
                    Replacement(""),
                    FullDecl(false) { }

    DAndReplace(Decl *D, std::string R) : Declaration(D),
                                          Statement(nullptr),
                                          Replacement(R),
                                          FullDecl(false) {}

    DAndReplace(Decl *D, std::string R, bool F) : Declaration(D),
                                                  Statement(nullptr),
                                                  Replacement(R),
                                                  FullDecl(F) {}


    DAndReplace(Decl *D, DeclStmt *S, std::string R) : Declaration(D),
                                                       Statement(S),
                                                       Replacement(R),
                                                       FullDecl(false) { }

    template<typename T>
    bool hasDeclType() const {
      static_assert(std::is_same<T, VarDecl>() || std::is_same<T, ParmVarDecl>()
                        || std::is_same<T, FunctionDecl>()
                        || std::is_same<T, FieldDecl>(),
          "Type is not supported Decl type.");
      return isa<T>(Declaration);
    }

    template<typename T>
    T *getDecl() const {
      static_assert(std::is_same<T, VarDecl>() || std::is_same<T, ParmVarDecl>()
                        || std::is_same<T, FunctionDecl>()
                        || std::is_same<T, FieldDecl>(),
                    "Type is not supported Decl type.");
      return dyn_cast<T>(Declaration);
    }
};

// Compare two DAndReplace values. The algorithm for comparing them relates
// their source positions. If two DAndReplace values refer to overlapping
// source positions, then they are the same. Otherwise, they are ordered
// by their placement in the input file.
//
// There are two special cases: Function declarations, and DeclStmts. In turn:
//
//  - Function declarations might either be a DAndReplace describing the entire
//    declaration, i.e. replacing "int *foo(void)"
//    with "int *foo(void) : itype(_Ptr<int>)". Or, it might describe just
//    replacing only the return type, i.e. "_Ptr<int> foo(void)". This is
//    discriminated against with the 'fullDecl' field of the DAndReplace type
//    and the comparison function first checks if the operands are
//    FunctionDecls and if the 'fullDecl' field is set.
//  - A DeclStmt of mupltiple Decls, i.e. 'int *a = 0, *b = 0'. In this case,
//    we want the DAndReplace to refer only to the specific sub-region that
//    would be replaced, i.e. '*a = 0' and '*b = 0'. To do that, we traverse
//    the Decls contained in a DeclStmt and figure out what the appropriate
//    source locations are to describe the positions of the independent
//    declarations.
struct DComp
{
    SourceManager &SM;
    DComp(SourceManager &S) : SM(S) { }

    SourceRange getWholeSR(SourceRange Orig, DAndReplace Dr) const;

    bool operator()(const DAndReplace Lhs, const DAndReplace Rhs) const;
};

typedef std::set<DAndReplace, DComp> RSet;

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
  void addGlobalDecl(VarDecl *VD, std::set<VarDecl *> *VDSet = nullptr);

  std::set<VarDecl *> &getVarsOnSameLine(VarDecl *VD);

  virtual ~GlobalVariableGroups();

private:
  SourceManager &SM;
  std::map<VarDecl *, std::set<VarDecl *>*> GlobVarGroups;
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