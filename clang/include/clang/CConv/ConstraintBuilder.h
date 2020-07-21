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

// Stores the concrete type that type variables are instantiated. This map has
// an entry for every call expression where the callee is has a generically
// typed parameter. The values in the map are another maps from type variable
// index in the called function's parameter list to the type the type variable
// becomes (or null if it is not used consistently).
typedef std::map<CallExpr *, std::map<unsigned int, const clang::Type *>>
    TypeVariableBindingsMapT;

class ConstraintBuilderConsumer : public clang::ASTConsumer {
public:
  explicit ConstraintBuilderConsumer(ProgramInfo &I, clang::ASTContext *C) :
    Info(I) { }

  virtual void HandleTranslationUnit(clang::ASTContext &);

private:
  ProgramInfo &Info;
  void SetProgramInfoTypeVars(TypeVariableBindingsMapT TypeVariableBindings,
                              ASTContext &C);
};

#endif
