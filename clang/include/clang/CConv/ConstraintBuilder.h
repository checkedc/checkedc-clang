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

class ConstraintBuilderConsumer : public clang::ASTConsumer {
public:
  explicit ConstraintBuilderConsumer(ProgramInfo &I, clang::ASTContext *C) :
    Info(I), LastRecordLoc(-1) { }

  virtual void HandleTranslationUnit(clang::ASTContext &);

private:
  ProgramInfo &Info;
  // stores the raw encoding of the begin location of the most recently
  // processed RecordDecl. This location is compared against subsequent VarDecl
  // locations, and if so, the flag InLineStructEncountered is set in ProgramInfo
  int LastRecordLoc;
};

#endif
