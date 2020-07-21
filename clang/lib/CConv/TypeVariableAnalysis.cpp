//=--TypeVariableAnalysis.cpp-------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Classes dealing with the processing of type variables for generic functions.
//===----------------------------------------------------------------------===//

#include "clang/CConv/TypeVariableAnalysis.h"

std::set<ConstraintVariable *> &TypeVariableEntry::getConstraintVariables() {
  assert("Accessing ConstraintVariable set for inconsistent Type Variable." &&
      IsConsistent);
  return ArgConsVars;
}

void TypeVariableEntry::insertConstraintVariables
    (std::set<ConstraintVariable *> &CVs) {
  assert("Accessing ConstraintVariable set for inconsistent Type Variable." &&
      IsConsistent);
  ArgConsVars.insert(CVs.begin(), CVs.end());
}

void TypeVariableEntry::setTypeParamConsVar(ConstraintVariable *CV) {
  assert("Accessing constraint variable for inconsistent Type Variable." &&
      IsConsistent);
  assert("Setting constraint variable to null" && CV != nullptr);
  assert("Changing already set constraint variable" &&
      TypeParamConsVar == nullptr);
  TypeParamConsVar = CV;
}

void TypeVariableEntry::updateEntry(QualType Ty,
                                    std::set<ConstraintVariable *> &CVs) {
  QualType PointeeType = Ty->getPointeeType();
  if (isTypeAnonymous(PointeeType)) {
    // We'll need a name to provide the type arguments during rewriting, so
    // no anonymous things here.
    IsConsistent = false;
  } else if (IsConsistent && getType() != Ty) {
    // If it has previously been instantiated as a different type, its use
    // is not consistent.
    IsConsistent = false;
  } else if (IsConsistent) {
    // Type variable has been encountered before with the same type. Insert
    // new constraint variables.
    insertConstraintVariables(CVs);
  }
  // If none of the above branches are hit, then this was already inconsistent,
  // so there's no need to update anything.
}

ConstraintVariable *TypeVariableEntry::getTypeParamConsVar() {
  assert("Accessing constraint variable for inconsistent Type Variable." &&
      IsConsistent);
  assert("Accessing null constraint variable" && TypeParamConsVar != nullptr);
  return TypeParamConsVar;
}

QualType TypeVariableEntry::getType() {
  assert("Accessing Type for inconsistent Type Variable." &&
      IsConsistent);
  return TyVarType;
}

bool TypeVariableEntry::getIsConsistent() {
  return IsConsistent;
}