//=--StructInit.h-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains functions and classes that deal with
// adding initializers to struct variables during the rewriting phase
//===----------------------------------------------------------------------===//

#ifndef _STRUCTINIT_H
#define _STRUCTINIT_H

#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ArrayBoundsInferenceConsumer.h"
#include "clang/3C/CheckedRegions.h"
#include "clang/3C/ConstraintResolver.h"
#include "clang/3C/MappingVisitor.h"
#include "clang/3C/RewriteUtils.h"
#include "clang/3C/StructInit.h"
#include "clang/3C/Utils.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Tooling/Refactoring/SourceCode.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>

using namespace clang;
using namespace llvm;

class StructVariableInitializer
    : public RecursiveASTVisitor<StructVariableInitializer> {
public:
  explicit StructVariableInitializer(ASTContext *_C, ProgramInfo &_I, RSet &R)
      : Context(_C), I(_I), RewriteThese(R), RecordsWithCPointers() {}

  bool VisitDeclStmt(DeclStmt *S);

private:
  bool VariableNeedsInitializer(VarDecl *VD);
  void insertVarDecl(VarDecl *VD, DeclStmt *S);

  ASTContext *Context;
  ProgramInfo &I;
  RSet &RewriteThese;
  std::set<RecordDecl *> RecordsWithCPointers;
};
#endif