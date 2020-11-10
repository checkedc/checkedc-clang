//=--TypeVariableAnalysis.h---------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef _TYPEVARIABLEANALYSIS_H
#define _TYPEVARIABLEANALYSIS_H

#include "ConstraintResolver.h"
#include "ConstraintVariables.h"
#include "ProgramInfo.h"
#include <set>

class TypeVariableEntry {
public:
  // Note: does not initialize TyVarType!
  TypeVariableEntry() : IsConsistent(false), TypeParamConsVar(nullptr) {}
  TypeVariableEntry(QualType Ty, std::set<ConstraintVariable *> &CVs)
      : TypeParamConsVar(nullptr) {
    // We'll need a name to provide the type arguments during rewriting, so no
    // anonymous types are allowed.
    IsConsistent = (Ty->isPointerType() || Ty->isArrayType()) &&
                   !isTypeAnonymous(Ty->getPointeeOrArrayElementType());
    TyVarType = Ty;
    ArgConsVars = CVs;
  }

  bool getIsConsistent() const;
  QualType getType();
  std::set<ConstraintVariable *> &getConstraintVariables();
  ConstraintVariable *getTypeParamConsVar();

  void insertConstraintVariables(std::set<ConstraintVariable *> &CVs);
  void setTypeParamConsVar(ConstraintVariable *CV);
  void updateEntry(QualType Ty, std::set<ConstraintVariable *> &CVs);

private:
  // Is this type variable instantiated consistently. True when all uses have
  // the same type and false otherwise.
  bool IsConsistent;

  // The type that this type variable is instantiated consistently as. The value
  // of this field should considered undefined if IsConsistent is false
  // (enforced in getter).
  QualType TyVarType;

  // Collection of constraint variables generated for all uses of the type
  // variable. Also should not be used when IsConsistent is false.
  std::set<ConstraintVariable *> ArgConsVars;

  // A single constraint variable for solving the checked type of the type
  // variable. It is constrained GEQ all elements of ArgConsVars.
  ConstraintVariable *TypeParamConsVar;
};

// Stores the instantiated type for each type variables. This map has
// an entry for every call expression where the callee is has a generically
// typed parameter. The values in the map are another maps from type variable
// index in the called function's parameter list to the type the type variable
// becomes (or null if it is not used consistently).
typedef std::map<CallExpr *, std::map<unsigned int, TypeVariableEntry>>
    TypeVariableMapT;

// Abstract class exposing methods for accessing the type variable map in a
// TypeVarVisitor.
class TypeVarInfo {
public:
  virtual void getConsistentTypeParams(CallExpr *CE,
                                       std::set<unsigned int> &Types) = 0;
  virtual void setProgramInfoTypeVars() = 0;
};

class TypeVarVisitor : public RecursiveASTVisitor<TypeVarVisitor>,
                       public TypeVarInfo {
public:
  explicit TypeVarVisitor(ASTContext *C, ProgramInfo &I)
      : Context(C), Info(I), CR(Info, Context), TVMap() {}

  bool VisitCastExpr(CastExpr *CE);
  bool VisitCallExpr(CallExpr *CE);

  void getConsistentTypeParams(CallExpr *CE, std::set<unsigned int> &Types);
  void setProgramInfoTypeVars();

private:
  ASTContext *Context;
  ProgramInfo &Info;
  ConstraintResolver CR;
  TypeVariableMapT TVMap;

  void insertBinding(CallExpr *CE, const TypeVariableType *TyV, QualType Ty,
                     CVarSet &CVs);
};
#endif