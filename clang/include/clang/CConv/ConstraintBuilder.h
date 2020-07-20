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

class TypeVariableEntry {
public:
  // Note: does not initialize TyVarType!
  TypeVariableEntry() :
      IsConsistent(false) {}
  TypeVariableEntry(QualType Ty) :
      IsConsistent(true), TyVarType(Ty) {}

  void makeInconsistent();
  bool getIsConsistent();
  QualType getType();
  std::set<ConstraintVariable *> &getConstraintVariables();
  ConstraintVariable *getTypeParamConsVar();

  void insertConstraintVariables(std::set<ConstraintVariable *> &CVs);
  void setTypeParamConsVar(ConstraintVariable *CV);

private:
  // Is this type variable used consistently. True when all uses have the same
  // type and false otherwise.
  bool IsConsistent;

  // The type that this type variable is used consistently as. The value of this
  // field should considered undefined if IsConsistent is false (enforced in
  // getter).
  QualType TyVarType;

  // Collection of constraint variables generated for all uses of the type
  // variable. Also should not be used when IsConsistent is false.
  std::set<ConstraintVariable *> ArgConsVars;

  ConstraintVariable *TypeParamConsVar;
};

typedef std::map<CallExpr *, std::map<unsigned int, TypeVariableEntry>>
    TypeVariableMapT;

class ConstraintBuilderConsumer : public clang::ASTConsumer {
public:
  explicit ConstraintBuilderConsumer(ProgramInfo &I, clang::ASTContext *C) :
    Info(I) { }

  virtual void HandleTranslationUnit(clang::ASTContext &);

private:
  ProgramInfo &Info;
  void SetProgramInfoTypeVars(TypeVariableMapT TypeVariableBindings,
                              ASTContext &C);
};

#endif
