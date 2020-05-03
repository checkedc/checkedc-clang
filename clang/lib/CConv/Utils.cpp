//=--Utils.cpp----------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of Utils methods.
//===----------------------------------------------------------------------===//

#include "llvm/Support/Path.h"

#include "clang/CConv/Utils.h"
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/ConstraintVariables.h"

using namespace llvm;
using namespace clang;

const clang::Type *getNextTy(const clang::Type *Ty) {
  if (Ty->isPointerType()) {
    // TODO: how to keep the qualifiers around, and what qualifiers do
    //       we want to keep?
    QualType qtmp = Ty->getLocallyUnqualifiedSingleStepDesugaredType();
    return qtmp.getTypePtr()->getPointeeType().getTypePtr();
  }
  else
    return Ty;
}

ConstraintVariable *getHighest(std::set<ConstraintVariable *> Vs,
                               ProgramInfo &Info) {
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
  if (!FD->isThisDeclarationADefinition()) {
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
  if (FD->isThisDeclarationADefinition() && FD->hasBody()) {
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

clang::CheckedPointerKind getCheckedPointerKind(InteropTypeExpr *ItypeExpr) {
  TypeSourceInfo *InteropTypeInfo = ItypeExpr->getTypeInfoAsWritten();
  const clang::Type *InnerType = InteropTypeInfo->getType().getTypePtr();
  if (InnerType->isCheckedPointerNtArrayType()) {
    return CheckedPointerKind::NtArray;
  }
  if (InnerType->isCheckedPointerArrayType()) {
    return CheckedPointerKind::Array;
  }
  if (InnerType->isCheckedPointerType()) {
    return CheckedPointerKind::Ptr;
  }
  return CheckedPointerKind::Unchecked;
}

// Check if function body exists for the
// provided declaration.
bool hasFunctionBody(clang::Decl *D) {
  // If this a parameter?
  if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
    if (DeclContext *DC = PD->getParentFunctionOrMethod()) {
      FunctionDecl *FD = dyn_cast<FunctionDecl>(DC);
      if (getDefinition(FD) != nullptr) {
        return true;
      }
    }
    return false;
  }
  // Else this should be within body and
  // the function body should exist.
  return true;
}

static std::string storageClassToString(StorageClass SC) {
  switch (SC) {
    case StorageClass::SC_Static: return "static ";
    case StorageClass::SC_Extern: return "extern ";
    case StorageClass::SC_Register: return "register ";
    // For all other cases, we do not care.
    default: return "";
  }
}

// This method gets the storage qualifier for the
// provided declaration i.e., static, extern, etc.
std::string getStorageQualifierString(Decl *D) {
  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    return storageClassToString(FD->getStorageClass());
  }
  if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    return storageClassToString(VD->getStorageClass());
  }
  return "";
}

bool isNULLExpression(clang::Expr *E, ASTContext &C) {
  // This checks if the expression is NULL. Specifically, (void*)0
  if (CStyleCastExpr *CS = dyn_cast<CStyleCastExpr>(E)) {
    Expr *SE = CS->getSubExpr();

    return SE->isIntegerConstantExpr(C) &&
           SE->isNullPointerConstant(C,
                                     Expr::NPC_ValueDependentIsNotNull);
  }
  return false;
}

bool getAbsoluteFilePath(std::string FileName, std::string &AbsoluteFp) {
  // Get absolute path of the provided file
  // returns true if successful else false.
  SmallString<255> abs_path(FileName);
  llvm::sys::fs::make_absolute(BaseDir,abs_path);
  AbsoluteFp = abs_path.str();
  return true;
}

bool functionHasVarArgs(clang::FunctionDecl *FD) {
  if (FD && FD->getFunctionType()->isFunctionProtoType()) {
    const FunctionProtoType *srcType =
        dyn_cast<FunctionProtoType>(FD->getFunctionType());
    return srcType->isVariadic();
  }
  return false;
}

bool isFunctionAllocator(std::string FuncName) {
  return llvm::StringSwitch<bool>(FuncName)
    .Cases("malloc", "calloc", "realloc", true)
    .Default(false);
}

float getTimeSpentInSeconds(clock_t StartTime) {
  return float(clock() - StartTime)/CLOCKS_PER_SEC;
}

bool isPointerType(clang::VarDecl *VD) {
  return VD->getType().getTypePtr()->isPointerType();
}

bool isStructOrUnionType(clang::VarDecl *VD) {
  return VD->getType().getTypePtr()->isStructureType() ||
         VD->getType().getTypePtr()->isUnionType();
}

std::string tyToStr(const clang::Type *T) {
  QualType QT(T, 0);
  return QT.getAsString();
}

Expr *removeAuxillaryCasts(Expr *E) {
  bool NeedStrip = true;
  while (NeedStrip) {
    NeedStrip = false;
    E = E->IgnoreParenImpCasts();
    if (CStyleCastExpr *C = dyn_cast<CStyleCastExpr>(E)) {
      E = C->getSubExpr();
      NeedStrip = true;
    }
  }
  return E;
}


bool isTypeHasVoid(clang::QualType QT) {
  const clang::Type *CurrType = QT.getTypePtrOrNull();
  if (CurrType != nullptr) {
    if (CurrType->isVoidType())
      return true;
    const clang::Type *InnerType = getNextTy(CurrType);
    while (InnerType != CurrType) {
      CurrType = InnerType;
      InnerType = getNextTy(InnerType);
    }

    return InnerType->isVoidType();
  }
  return false;
}

bool isVarArgType(const std::string &TypeName) {
  return TypeName == "struct __va_list_tag *" || TypeName == "va_list" ||
         TypeName == "struct __va_list_tag";
}

bool hasVoidType(clang::ValueDecl *D) {
  return isTypeHasVoid(D->getType());
}

bool canWrite(const std::string &FilePath) {
  // Was this file explicitly provided on the command line?
  if (FilePaths.count(FilePath) > 0)
    return true;
  // Get the absolute path of the file and check that
  // the file path starts with the base directory.
  std::string fileAbsPath = FilePath;
  getAbsoluteFilePath(FilePath, fileAbsPath);
  return fileAbsPath.rfind(BaseDir, 0) == 0;
}

unsigned longestCommonSubsequence(const char *Str1, const char *Str2,
                                  unsigned Str1Len, unsigned Str2Len) {
  if (Str1Len == 0 || Str2Len == 0)
    return 0;
  if (Str1[Str1Len - 1] == Str2[Str2Len - 1])
    return 1 + longestCommonSubsequence(Str1, Str2, Str1Len - 1, Str2Len - 1);
  else
    return std::max(longestCommonSubsequence(Str1, Str2, Str1Len, Str2Len - 1),
                    longestCommonSubsequence(Str1, Str2, Str1Len - 1, Str2Len));
}