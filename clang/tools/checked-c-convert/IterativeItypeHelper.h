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

// this method resets the constraint graph by removing
// equality edges involving itype variables.
unsigned long resetWithitypeConstraints(Constraints &CS);

// identify the functions which have the constraint variables of parameters
// or return changed from previous iteration.
bool identifyModifiedFunctions(Constraints &CS, std::set<std::string> &modifiedFunctions);

// This method detects and updates the newly detected (in the previous iteration)
// itype parameters and return values for all the provided set of functions (modifiedFunctions).
// Note that, these are the detections made by the tool, i.e., not the ones provided by user
unsigned long detectAndUpdateITypeVars(ProgramInfo &Info, std::set<std::string> &modifiedFunctions);

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
