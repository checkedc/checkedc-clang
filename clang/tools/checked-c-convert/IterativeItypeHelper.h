//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//

#ifndef _ITYPECONSTRAINTDETECTOR_H
#define _ITYPECONSTRAINTDETECTOR_H

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "ProgramInfo.h"

using namespace clang;

unsigned long detectAndUpdateITypeVars(ProgramInfo &Info);

class FVConstraintDetectorVisitor : public RecursiveASTVisitor<FVConstraintDetectorVisitor> {
public:
  explicit FVConstraintDetectorVisitor(ASTContext *C, ProgramInfo &I, std::set<std::string> &V)
    : Context(C), Info(I), VisitedSet(V) {}

  bool VisitFunctionDecl(FunctionDecl *);
private:
  ASTContext            *Context;
  ProgramInfo           &Info;
  std::set<std::string> &VisitedSet;
};



class FVConstraintDetectorConsumer : public ASTConsumer {
public:
  explicit FVConstraintDetectorConsumer(ProgramInfo &I, ASTContext *C) :
  Info(I) { }

  virtual void HandleTranslationUnit(ASTContext &Context);

private:
  ProgramInfo &Info;
};

#endif //_ITYPECONSTRAINTDETECTOR_H
