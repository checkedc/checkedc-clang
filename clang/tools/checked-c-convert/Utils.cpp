//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of Utils methods.
//===----------------------------------------------------------------------===//
#include "llvm/Support/Path.h"

#include "Utils.h"
#include "ConstraintVariables.h"

using namespace clang;

static std::map<std::string, std::string> AbsoluteFilePathCache;

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

// Walk the list of declarations and find a declaration that is NOT a
// definition and does NOT have a body.
FunctionDecl *getDeclaration(FunctionDecl *FD) {
  // optimization: if the provided Decl is itself a declaration then
  // return the same Decl
  if (!FD->isThisDeclarationADefinition())
    return FD;

  for (const auto &D : FD->redecls())
    if (FunctionDecl *tFD = dyn_cast<FunctionDecl>(D))
      if (!tFD->isThisDeclarationADefinition())
        return tFD;

  return nullptr;
}

// Walk the list of declarations and find a declaration accompanied by
// a definition and a function body.
FunctionDecl *getDefinition(FunctionDecl *FD) {
  // optimization: if the provided Decl is itself associated with a function
  // body return the same Decl
  if (FD->isThisDeclarationADefinition() && FD->hasBody())
    return FD;

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
  const clang::PointerType *ptrType = itypeExpr->getType().getTypePtr()->getAs<PointerType>();
  assert(ptrType != nullptr && "itype used for non-pointer value.");
  return ptrType->getKind();
}

// check if the provided declaration is a function parameter and is part of
// a declaration only function
bool isDeclarationParam(clang::Decl *param) {
  // if this a parameter?
  if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(param))
    if (DeclContext *DC = PD->getParentFunctionOrMethod()) {
      FunctionDecl *FD = dyn_cast<FunctionDecl>(DC);
      return getDefinition(FD) == nullptr;
    }
  // else this is not a parameter
  return false;
}

static std::string storageClassToString(StorageClass SC) {
  switch (SC) {
    default: return "";
    case StorageClass::SC_Static: return "static ";
    case StorageClass::SC_Extern: return "extern ";
    case StorageClass::SC_Register: return "register ";
  }
}

// this method gets the storage qualifier for the provided declaration
// i.e., static, extern, etc.
std::string getStorageQualifierString(Decl *D) {
  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D))
    return storageClassToString(FD->getStorageClass());

  if (VarDecl *VD = dyn_cast<VarDecl>(D))
    return storageClassToString(VD->getStorageClass());

  return "";
}

bool getAbsoluteFilePath(std::string fileName, std::string &absoluteFP) {
  // get absolute path of the provided file returns true if successful else
  // false check the cache.
  if (AbsoluteFilePathCache.find(fileName) == AbsoluteFilePathCache.end()) {
    SmallString<255> abs_path(fileName);
    std::error_code ec = llvm::sys::fs::make_absolute(abs_path);
    if (ec)
      return false;
    AbsoluteFilePathCache[fileName] = abs_path.str();
  }
  absoluteFP = AbsoluteFilePathCache[fileName];
  return true;
}

bool functionHasVarArgs(clang::FunctionDecl *FD) {
  if (FD && FD->getFunctionType()->isFunctionProtoType()) {
    const FunctionProtoType *srcType = dyn_cast<FunctionProtoType>(FD->getFunctionType());
    return srcType->isVariadic();
  }
  return false;
}

bool isFunctionAllocator(std::string funcName) {
  return llvm::StringSwitch<bool>(funcName)
    .Cases("malloc", "calloc", "realloc", true)
    .Default(false);
}

float getTimeSpentInSeconds(clock_t startTime) {
  return float(clock() - startTime)/CLOCKS_PER_SEC;
}

bool isPointerType(clang::VarDecl *VD) {
  return VD->getType().getTypePtr()->isPointerType();
}

bool isStructOrUnionType(clang::VarDecl *VD) {
  return VD->getType().getTypePtr()->isStructureType() || VD->getType().getTypePtr()->isUnionType();
}

std::string tyToStr(const Type *T) {
  QualType QT(T, 0);
  return QT.getAsString();
}
