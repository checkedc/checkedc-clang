//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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
    Stmt        *Statement;   // The Stmt, if it exists.
    std::string Replacement;  // The string to replace the declaration with.
    bool        fullDecl;     // If the declaration is a function, true if
    // replace the entire declaration or just the
    // return declaration.
    DAndReplace() : Declaration(nullptr),
                    Statement(nullptr),
                    Replacement(""),
                    fullDecl(false) { }

    DAndReplace(Decl *D, std::string R) : Declaration(D),
                                          Statement(nullptr),
                                          Replacement(R),
                                          fullDecl(false) {}

    DAndReplace(Decl *D, std::string R, bool F) : Declaration(D),
                                                  Statement(nullptr),
                                                  Replacement(R),
                                                  fullDecl(F) {}


    DAndReplace(Decl *D, Stmt *S, std::string R) :  Declaration(D),
                                                    Statement(S),
                                                    Replacement(R),
                                                    fullDecl(false) { }
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

    SourceRange getWholeSR(SourceRange orig, DAndReplace dr) const;

    bool operator()(const DAndReplace lhs, const DAndReplace rhs) const;
};

typedef std::set<DAndReplace, DComp> RSet;

void rewrite(ParmVarDecl *PV, Rewriter &R, std::string sRewrite);

void rewrite( VarDecl               *VD,
              Rewriter              &R,
              std::string           sRewrite,
              Stmt                  *WhereStmt,
              RSet                  &skip,
              const DAndReplace     &N,
              RSet                  &toRewrite,
              ASTContext            &A);

// Visit each Decl in toRewrite and apply the appropriate pointer type
// to that Decl. The state of the rewrite is contained within R, which
// is both input and output. R is initialized to point to the 'main'
// source file for this transformation. toRewrite contains the set of
// declarations to rewrite. S is passed for source-level information
// about the current compilation unit. skip indicates some rewrites that
// we should skip because we already applied them, for example, as part
// of turning a single line declaration into a multi-line declaration.
void rewrite( Rewriter              &R,
              RSet                  &toRewrite,
              RSet                  &skip,
              SourceManager         &S,
              ASTContext            &A,
              std::set<FileID>      &Files);


// Class for visiting declarations during re-writing to find locations to
// insert casts. Right now, it looks specifically for 'free'.
class CastPlacementVisitor : public RecursiveASTVisitor<CastPlacementVisitor> {
public:
  explicit CastPlacementVisitor(ASTContext *C, ProgramInfo &I, Rewriter &R,
                                RSet &DR, std::set<FileID> &Files, std::set<std::string> &V)
          : Context(C), R(R), Info(I), rewriteThese(DR), Files(Files), VisitedSet(V) {}

  bool VisitCallExpr(CallExpr *);
  bool VisitFunctionDecl(FunctionDecl *);
private:
  std::set<unsigned int> getParamsForExtern(std::string);
  // get existing itype string from constraint variables.
  // if tries to get the string from declaration, however,
  // if there is no declaration of the function,
  // it will try to get it from the definition.
  std::string getExistingIType(ConstraintVariable *decl, ConstraintVariable *defn,
                               FunctionDecl *funcDecl);
  bool anyTop(std::set<ConstraintVariable*>);
  ASTContext            *Context;
  Rewriter              &R;
  ProgramInfo           &Info;
  RSet                  &rewriteThese;
  std::set<FileID>      &Files;
  std::set<std::string> &VisitedSet;
};


class RewriteConsumer : public ASTConsumer {
public:
  explicit RewriteConsumer(ProgramInfo &I,
                           std::set<std::string> &F, ASTContext *Context, std::string &OPostfix, std::string &bDir) :
                           Info(I), InOutFiles(F), OutputPostfix(OPostfix), BaseDir(bDir) {}

  virtual void HandleTranslationUnit(ASTContext &Context);

private:
  ProgramInfo &Info;
  std::set<std::string> &InOutFiles;
  std::string &OutputPostfix;
  std::string &BaseDir;
};

#endif //_REWRITEUTILS_H
