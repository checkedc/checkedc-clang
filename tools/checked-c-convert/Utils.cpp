//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of Utils methods.
//===----------------------------------------------------------------------===//

#include "Utils.h"
#include "ConstraintVariables.h"

using namespace clang;

const clang::Type *getNextTy(const clang::Type *Ty) {
  if(Ty->isPointerType()) {
    // TODO: how to keep the qualifiers around, and what qualifiers do
    //       we want to keep?
    QualType qtmp = Ty->getLocallyUnqualifiedSingleStepDesugaredType();
    return qtmp.getTypePtr()->getPointeeType().getTypePtr();
  }
  else
    return Ty;
}

ConstraintVariable *getHighest(std::set<ConstraintVariable*> Vs, ProgramInfo &Info) {
  if (Vs.size() == 0)
    return nullptr;

  ConstraintVariable *V = nullptr;

  for (auto &P : Vs) {
    if (V) {
      if (V->isLt(*P, Info))
        V = P;
    } else {
      V = P;
    }
  }

  return V;
}

// Walk the list of declarations and find a declaration that is NOT
// a definition and does NOT have a body.
FunctionDecl *getDeclaration(FunctionDecl *FD) {
  for (const auto &D : FD->redecls())
    if (FunctionDecl *tFD = dyn_cast<FunctionDecl>(D))
      if (!tFD->isThisDeclarationADefinition())
        return tFD;

  return FD;
}

// Walk the list of declarations and find a declaration accompanied by
// a definition and a function body.
FunctionDecl *getDefinition(FunctionDecl *FD) {
  for (const auto &D : FD->redecls())
    if (FunctionDecl *tFD = dyn_cast<FunctionDecl>(D))
      if (tFD->isThisDeclarationADefinition() && tFD->hasBody())
        return tFD;

  return nullptr;
}