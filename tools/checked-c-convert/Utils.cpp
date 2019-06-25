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
  // optimization
  if(!FD->isThisDeclarationADefinition()) {
    return FD;
  }
  for (const auto &D : FD->redecls())
    if (FunctionDecl *tFD = dyn_cast<FunctionDecl>(D))
      if (!tFD->isThisDeclarationADefinition())
        return tFD;

  return nullptr;
}

// Walk the list of declarations and find a declaration accompanied by
// a definition and a function body.
FunctionDecl *getDefinition(FunctionDecl *FD) {
  // optimization
  if(FD->isThisDeclarationADefinition() && FD->hasBody()) {
    return FD;
  }
  for (const auto &D : FD->redecls())
    if (FunctionDecl *tFD = dyn_cast<FunctionDecl>(D))
      if (tFD->isThisDeclarationADefinition() && tFD->hasBody())
        return tFD;

  return nullptr;
}

SourceLocation
getFunctionDeclarationEnd(FunctionDecl *FD, SourceManager &S)
{
  const FunctionDecl *oFD = nullptr;

  if (FD->hasBody(oFD) && oFD == FD) {
    // Replace everything up to the beginning of the body.
    const Stmt *Body = FD->getBody(oFD);

    int Offset = 0;
    const char *Buf = S.getCharacterData(Body->getSourceRange().getBegin());

    while (*Buf != ')') {
      Buf--;
      Offset--;
    }

    return Body->getSourceRange().getBegin().getLocWithOffset(Offset);
  } else {
    return FD->getSourceRange().getEnd();
  }
}

clang::CheckedPointerKind getCheckedPointerKind(InteropTypeExpr *itypeExpr) {
  TypeSourceInfo * interopTypeInfo = itypeExpr->getTypeInfoAsWritten();
  const clang::Type *innerType = interopTypeInfo->getType().getTypePtr();
  if(innerType->isCheckedPointerNtArrayType()) {
    return CheckedPointerKind ::NtArray;
  }
  if(innerType->isCheckedPointerArrayType()) {
    return CheckedPointerKind ::Array;
  }
  if(innerType->isCheckedPointerType()) {
    return CheckedPointerKind ::Ptr;
  }
  return CheckedPointerKind::Unchecked;
}

// check if function body exists for the
// provided declaration.
bool hasFunctionBody(clang::Decl *param) {
  // if this a parameter?
  if(ParmVarDecl *PD = dyn_cast<ParmVarDecl>(param)) {
    if(DeclContext *DC = PD->getParentFunctionOrMethod()) {
      FunctionDecl *FD = dyn_cast<FunctionDecl>(DC);
      if (getDefinition(FD) != nullptr) {
        return true;
      }
    }
    return false;
  }
  // else this should be within body and
  // the function body should exist.
  return true;
}

// check if the provided expression is
// a function pointer.
bool isFunctionPointerExpr(clang::Expr *toCheck) {
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(toCheck)) {
    return dyn_cast<FunctionDecl>(DRE->getDecl()) != nullptr;
  }
  return false;
}