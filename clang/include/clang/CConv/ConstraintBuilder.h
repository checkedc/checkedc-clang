//=--ConstraintBuilder.h------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef _CONSTRAINT_BUILDER
#define _CONSTRAINT_BUILDER
#include "clang/AST/ASTConsumer.h"

#include "ProgramInfo.h"

// Here, ConsGen is a function pointer that given 2 atoms generates
// corresponding constraint with it. This allows callers to specialize the
// way (i.e., either Eq or Geq) they want to generate constraints.
void constrainConsVar(std::set<ConstraintVariable *> &RHS,
                      std::set<ConstraintVariable *> &LHS, ProgramInfo &Info,
                      Stmt *S,
                      ASTContext *C,
                      ConsGenFuncType ConsGen = EqConsGenerator,
                      bool FuncCall = false);
void constrainConsVar(ConstraintVariable *LHS,
                      ConstraintVariable *RHS, ProgramInfo &Info,
                      Stmt *S,
                      ASTContext *C,
                      ConsGenFuncType ConsGen = EqConsGenerator,
                      bool FuncCall = false);

class ConstraintBuilderConsumer : public clang::ASTConsumer {
public:
  explicit ConstraintBuilderConsumer(ProgramInfo &I, clang::ASTContext *C) :
    Info(I) { }

  virtual void HandleTranslationUnit(clang::ASTContext &);

private:
  ProgramInfo &Info;
};

#endif
