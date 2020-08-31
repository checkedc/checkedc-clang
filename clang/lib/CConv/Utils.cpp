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
  QualType Typ = E->getType();
  E = removeAuxillaryCasts(E);
  return Typ->isPointerType() && E->isIntegerConstantExpr(C) &&
         E->isNullPointerConstant(C,Expr::NPC_ValueDependentIsNotNull);
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
  return std::find(AllocatorFunctions.begin(), AllocatorFunctions.end(),
                   FuncName) != AllocatorFunctions.end()
         || llvm::StringSwitch<bool>(FuncName)
             .Cases("malloc", "calloc", "realloc", true)
             .Default(false);
}

float getTimeSpentInSeconds(clock_t StartTime) {
  return float(clock() - StartTime)/CLOCKS_PER_SEC;
}

bool isPointerType(clang::ValueDecl *VD) {
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

//Expr *getNormalizedExpr(Expr *CE) {
//  while (true) {
//    if (CHKCBindTemporaryExpr *E = dyn_cast<CHKCBindTemporaryExpr>(CE)) {
//      CE = E->getSubExpr();
//      continue;
//    }
//    if (ParenExpr *E = dyn_cast <ParenExpr>(CE)) {
//      CE = E->getSubExpr();
//      continue;
//    }
//    break;
//  }
//  return CE;
//}

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

//// Check the equality of VTy and UTy. There are some specific rules that
//// fire, and a general check is yet to be implemented.
//bool checkStructuralEquality(std::set<ConstraintVariable *> V,
//                                          std::set<ConstraintVariable *> U,
//                                          QualType VTy,
//                                          QualType UTy)
//{
//  // First specific rule: Are these types directly equal?
//  if (VTy == UTy) {
//    return true;
//  } else {
//    // Further structural checking is TODO.
//    return false;
//  }
//}
//
//bool checkStructuralEquality(QualType D, QualType S) {
//  if (D == S)
//    return true;
//
//  return D->isPointerType() == S->isPointerType();
//}

static bool CastCheck(clang::QualType DstType,
                      clang::QualType SrcType) {

  // Check if both types are same.
  if (SrcType == DstType)
    return true;

  const clang::Type *SrcTypePtr = SrcType.getCanonicalType().getTypePtr();
  const clang::Type *DstTypePtr = DstType.getCanonicalType().getTypePtr();

  const clang::PointerType *SrcPtrTypePtr =
      dyn_cast<clang::PointerType>(SrcTypePtr);
  const clang::PointerType *DstPtrTypePtr =
      dyn_cast<clang::PointerType>(DstTypePtr);

  // Both are pointers? check their pointee
  if (SrcPtrTypePtr && DstPtrTypePtr) {
    return (SrcPtrTypePtr->isVoidPointerType()) ||
        CastCheck(DstPtrTypePtr->getPointeeType(),
                  SrcPtrTypePtr->getPointeeType());
  }

  if (SrcPtrTypePtr || DstPtrTypePtr)
    return false;

  // If both are not scalar types? Then the types must be exactly same.
  if (!(SrcTypePtr->isScalarType() && DstTypePtr->isScalarType()))
    return SrcTypePtr == DstTypePtr;

  // Check if both types are compatible.
  bool BothNotChar = SrcTypePtr->isCharType() ^ DstTypePtr->isCharType();
  bool BothNotInt =
      (SrcTypePtr->isIntegerType() && SrcTypePtr->isUnsignedIntegerType())
      ^ (DstTypePtr->isIntegerType() && DstTypePtr->isUnsignedIntegerType());
  bool BothNotFloat =
      SrcTypePtr->isFloatingType() ^ DstTypePtr->isFloatingType();

  return !(BothNotChar || BothNotInt || BothNotFloat);
}

bool isCastSafe(clang::QualType DstType,
                clang::QualType SrcType) {
  const clang::Type *DstTypePtr = DstType.getTypePtr();
  const clang::PointerType *DstPtrTypePtr =
      dyn_cast<clang::PointerType>(DstTypePtr);
  if (!DstPtrTypePtr) // Safe to cast to a non-pointer.
    return true;
  else
    return CastCheck(DstType,SrcType);
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

bool isInSysHeader(clang::Decl *D) {
  if (D != nullptr) {
    auto &C = D->getASTContext();
    FullSourceLoc FL = C.getFullLoc(D->getBeginLoc());
    return FL.isInSystemHeader();
  }
  return false;
}

std::string getSourceText(const clang::SourceRange &SR,
                          const clang::ASTContext &C) {
  assert(SR.isValid() && "Invalid Source Range requested.");
  auto &SM = C.getSourceManager();
  auto LO = C.getLangOpts();
  llvm::StringRef Srctxt =
      Lexer::getSourceText(CharSourceRange::getTokenRange(SR), SM, LO);
  return Srctxt.str();
}

unsigned longestCommonSubsequence(const char *Str1, const char *Str2,
                                  unsigned long Str1Len, unsigned long Str2Len) {
  if (Str1Len == 0 || Str2Len == 0)
    return 0;
  if (Str1[Str1Len - 1] == Str2[Str2Len - 1])
    return 1 + longestCommonSubsequence(Str1, Str2, Str1Len - 1, Str2Len - 1);
  else
    return std::max(longestCommonSubsequence(Str1, Str2, Str1Len, Str2Len - 1),
                    longestCommonSubsequence(Str1, Str2, Str1Len - 1, Str2Len));
}

// Get the type variable used in a parameter declaration, or return null if no
// type variable is used.
const TypeVariableType *getTypeVariableType(DeclaratorDecl *Decl){
  // This makes a lot of assumptions about how the AST will look.
  if (auto *ITy = Decl->getInteropTypeExpr()){
    const auto *Ty = ITy->getType().getTypePtr();
    if (Ty && Ty->isPointerType()) {
      auto *PtrTy = Ty->getPointeeType().getTypePtr();
      if (auto *TypdefTy = dyn_cast_or_null<TypedefType>(PtrTy))
        return dyn_cast<TypeVariableType>(TypdefTy->desugar());
    }
  }
  return nullptr;
}

bool isTypeAnonymous(const clang::Type *T) {
  return T->isRecordType() && !(T->getAsRecordDecl()->getIdentifier()
      || T->getAsRecordDecl()->getTypedefNameForAnonDecl());
}

unsigned int getParameterIndex(ParmVarDecl *PV, FunctionDecl *FD) {
  // This is kind of hacky, maybe we should record the index of the
  // parameter when we find it, instead of re-discovering it here.
  unsigned int PIdx = 0;
  for (const auto &I : FD->parameters()) {
    if (I == PV)
      return PIdx;
    PIdx++;
  }
  llvm_unreachable("Parameter declaration not found in function declaration.");
}

bool evaluateToInt(Expr *E, const ASTContext &C, int &Result) {
  Expr::EvalResult ER;
  E->EvaluateAsInt(ER, C, clang::Expr::SE_NoSideEffects, false);
  if (ER.Val.isInt()) {
    Result = ER.Val.getInt().getExtValue();
    return  true;
  }
  return false;
}

bool isZeroBoundsExpr(BoundsExpr *BE, const ASTContext &C) {
  if (auto *CBE = dyn_cast<CountBoundsExpr>(BE)){
    // count(0) and byte_count(0)
    Expr *E = CBE->getCountExpr();
    int Result;
    if (evaluateToInt(E, C, Result))
      return Result == 0;
  }
  // Range bounds and empty bounds are ignored. I suppose range bounds could be
  // size zero bounds, but checking this would be considerably more complicated
  // and it seems unlikely to show up in real code.
  return false;
}