//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#ifndef _CONSTRAINT_BUILDER
#define _CONSTRAINT_BUILDER
#include "clang/AST/ASTConsumer.h"

#include "ProgramInfo.h"

void constrainEq(std::set<ConstraintVariable*> &RHS,
                 std::set<ConstraintVariable*> &LHS, ProgramInfo &Info);
void constrainEq( ConstraintVariable *LHS, 
                  ConstraintVariable *RHS, ProgramInfo &Info);

class ConstraintBuilderConsumer : public clang::ASTConsumer {
public:
  explicit ConstraintBuilderConsumer(ProgramInfo &I, clang::ASTContext *C) :
    Info(I) { }

  virtual void HandleTranslationUnit(clang::ASTContext &);

private:
  ProgramInfo &Info;
};

#endif
