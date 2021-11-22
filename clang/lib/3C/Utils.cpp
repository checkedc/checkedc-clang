//=--Utils.cpp----------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of Utils methods.
//===----------------------------------------------------------------------===//

#include "clang/3C/Utils.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ConstraintVariables.h"
#include "clang/AST/FormatString.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/Path.h"
#include <errno.h>

using namespace llvm;
using namespace clang;

const clang::Type *getNextTy(const clang::Type *Ty) {
  if (Ty->isPointerType()) {
    // TODO: how to keep the qualifiers around, and what qualifiers do
    //       we want to keep?
    QualType Qtmp = Ty->getLocallyUnqualifiedSingleStepDesugaredType();
    return Qtmp.getTypePtr()->getPointeeType().getTypePtr();
  }
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
    if (FunctionDecl *TFD = dyn_cast<FunctionDecl>(D))
      if (!TFD->isThisDeclarationADefinition())
        return TFD;

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
    if (FunctionDecl *TFD = dyn_cast<FunctionDecl>(D))
      if (TFD->isThisDeclarationADefinition() && TFD->hasBody())
        return TFD;

  return nullptr;
}

//This should only be used as a fall back for when the clang library function
// FunctionTypeLoc::getRParenLoc cannot be called due to a null FunctionTypeLoc
// or for when the function returns an invalid source location.
SourceLocation getFunctionDeclRParen(FunctionDecl *FD, SourceManager &S) {
  const FunctionDecl *OFd = nullptr;
  if (FD->hasBody(OFd) && OFd == FD) {
    // Replace everything up to the beginning of the body.
    const Stmt *Body = FD->getBody(OFd);
    return locationPrecedingChar(Body->getBeginLoc(), S, ')');
  }
  return FD->getSourceRange().getEnd();
}

// Find the source location of the first character C preceding the given source
// location.  Because this uses the character buffer directly, it sees character
// data prior to preprocessing. This means characters that are in comments,
// macros or otherwise not part of the final preprocessed source code are seen
// and can cause this function to give an incorrect result. This should only be
// used as a fall back for when clang library function for obtaining source
// locations are not available or return invalid results.
SourceLocation locationPrecedingChar(SourceLocation SL, SourceManager &S,
                                     char C) {
  int Offset = 0;
  const char *Buf = S.getCharacterData(SL);
  while (*Buf != C) {
    Buf--;
    Offset--;
  }
  return SL.getLocWithOffset(Offset);
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

static std::string storageClassToString(StorageClass SC) {
  switch (SC) {
  case StorageClass::SC_Static:
    return "static ";
  case StorageClass::SC_Extern:
    return "extern ";
  case StorageClass::SC_Register:
    return "register ";
  // For all other cases, we do not care.
  default:
    return "";
  }
}

// This method gets the storage qualifier for the
// provided declaration including any trailing space, i.e., "static ",
// "extern ", etc., or "" if none.
std::string getStorageQualifierString(Decl *D) {
  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    return storageClassToString(FD->getStorageClass());
  }
  if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    return storageClassToString(VD->getStorageClass());
  }
  if (isa<TypedefDecl>(D)) {
    // `typedef` goes in the same syntactic position as a storage qualifier and
    // needs to be inserted when breaking up a multi-decl, just like a real
    // storage qualifier.
    return "typedef ";
  }
  return "";
}

bool isNULLExpression(clang::Expr *E, ASTContext &C) {
  QualType Typ = E->getType();
  E = removeAuxillaryCasts(E);
  return Typ->isPointerType() && E->isIntegerConstantExpr(C) &&
         E->isNullPointerConstant(C, Expr::NPC_ValueDependentIsNotNull);
}

std::error_code tryGetCanonicalFilePath(const std::string &FileName,
                                        std::string &AbsoluteFp) {
  SmallString<255> AbsPath;
  std::error_code EC;
  if (FileName.empty()) {
    // Strangely, llvm::sys::fs::real_path successfully returns the empty string
    // in this case. Return ENOENT, as realpath(3) would.
    EC = std::error_code(ENOENT, std::generic_category());
  } else {
    EC = llvm::sys::fs::real_path(FileName, AbsPath);
  }
  if (EC) {
    return EC;
  }
  AbsoluteFp = std::string(AbsPath.str());
  return EC;
}

bool filePathStartsWith(const std::string &Path, const std::string &Prefix) {
  // If the path exactly equals the prefix, don't ruin it by appending a
  // separator to the prefix. (This may never happen in 3C, but let's get it
  // right.)
  if (Prefix.empty() || Path == Prefix) {
    return true;
  }
  StringRef Separator = llvm::sys::path::get_separator();
  std::string PrefixWithTrailingSeparator = Prefix;
  if (!StringRef(Prefix).endswith(Separator)) {
    PrefixWithTrailingSeparator += Separator;
  }
  return Path.substr(0, PrefixWithTrailingSeparator.size()) ==
         PrefixWithTrailingSeparator;
}

bool functionHasVarArgs(clang::FunctionDecl *FD) {
  if (FD && FD->getFunctionType()->isFunctionProtoType()) {
    const FunctionProtoType *SrcType =
        FD->getFunctionType()->getAs<FunctionProtoType>();
    return SrcType->isVariadic();
  }
  return false;
}

bool isFunctionAllocator(std::string FuncName) {
  return std::find(_3COpts.AllocatorFunctions.begin(),
                   _3COpts.AllocatorFunctions.end(),
                   FuncName) != _3COpts.AllocatorFunctions.end() ||
         llvm::StringSwitch<bool>(FuncName)
             .Cases("malloc", "calloc", "realloc", true)
             .Default(false);
}

float getTimeSpentInSeconds(clock_t StartTime) {
  return float(clock() - StartTime) / CLOCKS_PER_SEC;
}

bool isPointerType(clang::ValueDecl *VD) {
  return VD->getType().getTypePtr()->isPointerType();
}

bool isPtrOrArrayType(const clang::QualType &QT) {
  return QT->isPointerType() || QT->isArrayType();
}

bool isNullableType(const clang::QualType &QT) {
  if (QT.getTypePtrOrNull())
    return QT->isPointerType() || QT->isArrayType() || QT->isIntegerType();
  return false;
}

bool canBeNtArray(const clang::QualType &QT) {
  // First get the canonical type so that the following checks will not have to
  // account for ParenType, DecayedType, or other variants used by clang.
  QualType Canon = QT.getCanonicalType();
  if (const auto &Ptr = dyn_cast<clang::PointerType>(Canon))
    return isNullableType(Ptr->getPointeeType());
  if (const auto &Arr = dyn_cast<clang::ArrayType>(Canon))
    return isNullableType(Arr->getElementType());
  return false;
}

bool isStructOrUnionType(clang::DeclaratorDecl *DD) {
  return DD->getType().getTypePtr()->isStructureType() ||
         DD->getType().getTypePtr()->isUnionType();
}

std::string qtyToStr(clang::QualType QT, const std::string &Name) {
  std::string S = Name;
  QT.getAsStringInternal(S, LangOptions());
  return S;
}

std::string tyToStr(const clang::Type *T, const std::string &Name) {
  return qtyToStr(QualType(T, 0), Name);
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

bool hasVoidType(clang::ValueDecl *D) { return isTypeHasVoid(D->getType()); }

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

static bool castCheck(clang::QualType DstType, clang::QualType SrcType) {

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
    return castCheck(DstPtrTypePtr->getPointeeType(),
                SrcPtrTypePtr->getPointeeType());
  }

  if (SrcPtrTypePtr || DstPtrTypePtr)
    return false;

  // Check function cast by comparing parameter and return types individually.
  const auto *SrcFnType = dyn_cast<clang::FunctionProtoType>(SrcTypePtr);
  const auto *DstFnType = dyn_cast<clang::FunctionProtoType>(DstTypePtr);
  if (SrcFnType && DstFnType) {
    if (SrcFnType->getNumParams() != DstFnType->getNumParams())
      return false;

    for (unsigned I = 0; I < SrcFnType->getNumParams(); I++)
      if (!castCheck(SrcFnType->getParamType(I),
                     DstFnType->getParamType(I)))
        return false;

    return castCheck(SrcFnType->getReturnType(), DstFnType->getReturnType());
  }

  // If both are not scalar types? Then the types must be exactly same.
  if (!(SrcTypePtr->isScalarType() && DstTypePtr->isScalarType()))
    return SrcTypePtr == DstTypePtr;

  // Check if both types are compatible.
  bool BothNotChar = SrcTypePtr->isCharType() ^ DstTypePtr->isCharType();
  bool BothNotInt =
      (SrcTypePtr->isIntegerType() && SrcTypePtr->isUnsignedIntegerType()) ^
      (DstTypePtr->isIntegerType() && DstTypePtr->isUnsignedIntegerType());
  bool BothNotFloat =
      SrcTypePtr->isFloatingType() ^ DstTypePtr->isFloatingType();

  return !(BothNotChar || BothNotInt || BothNotFloat);
}

bool isCastSafe(clang::QualType DstType, clang::QualType SrcType) {
  const clang::Type *DstTypePtr = DstType.getTypePtr();
  const clang::PointerType *DstPtrTypePtr =
      dyn_cast<clang::PointerType>(DstTypePtr);
  if (!DstPtrTypePtr) // Safe to cast to a non-pointer.
    return true;
  return castCheck(DstType, SrcType);
}

bool canWrite(const std::string &FilePath) {
  // Was this file explicitly provided on the command line?
  if (FilePaths.count(FilePath) > 0)
    return true;
  // Get the absolute path of the file and check that
  // the file path starts with the base directory.
  return filePathStartsWith(FilePath, _3COpts.BaseDir);
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
  return getSourceText(CharSourceRange::getTokenRange(SR), C);
}

std::string getSourceText(const clang::CharSourceRange &SR,
                          const clang::ASTContext &C) {
  assert(SR.isValid() && "Invalid Source Range requested.");
  auto &SM = C.getSourceManager();
  auto LO = C.getLangOpts();
  llvm::StringRef Srctxt = Lexer::getSourceText(SR, SM, LO);
  return Srctxt.str();
}

unsigned longestCommonSubsequence(const char *Str1, const char *Str2,
                                  unsigned long Str1Len,
                                  unsigned long Str2Len) {
  if (Str1Len == 0 || Str2Len == 0)
    return 0;
  if (Str1[Str1Len - 1] == Str2[Str2Len - 1])
    return 1 + longestCommonSubsequence(Str1, Str2, Str1Len - 1, Str2Len - 1);
  return std::max(longestCommonSubsequence(Str1, Str2, Str1Len, Str2Len - 1),
                  longestCommonSubsequence(Str1, Str2, Str1Len - 1, Str2Len));
}

bool isTypeAnonymous(const clang::Type *T) {
  return T->isRecordType() &&
         !(T->getAsRecordDecl()->getIdentifier() ||
           T->getAsRecordDecl()->getTypedefNameForAnonDecl());
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
    return true;
  }
  return false;
}

bool isZeroBoundsExpr(BoundsExpr *BE, const ASTContext &C) {
  if (auto *CBE = dyn_cast<CountBoundsExpr>(BE)) {
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

TypeLoc getBaseTypeLoc(TypeLoc T) {
  assert(!T.isNull() && "Can't get base location from Null.");
  while (!T.getNextTypeLoc().isNull() &&
         (!T.getAs<ParenTypeLoc>().isNull() ||
          T.getTypePtr()->isPointerType() || T.getTypePtr()->isArrayType()))
    T = T.getNextTypeLoc();
  return T;
}

Expr *ignoreCheckedCImplicit(Expr *E) {
  Expr *Old = nullptr;
  Expr *New = E;
  while (Old != New) {
    Old = New;
    New = Old->IgnoreExprTmp()->IgnoreImplicit();
  }
  return New;
}

FunctionTypeLoc getFunctionTypeLoc(TypeLoc TLoc) {
  TLoc = getBaseTypeLoc(TLoc);
  auto ATLoc = TLoc.getAs<AttributedTypeLoc>();
  if (!ATLoc.isNull())
    TLoc = ATLoc.getNextTypeLoc();
  return TLoc.getAs<FunctionTypeLoc>();
}

FunctionTypeLoc getFunctionTypeLoc(DeclaratorDecl *Decl) {
  if (auto *TSInfo = Decl->getTypeSourceInfo())
    return getFunctionTypeLoc(TSInfo->getTypeLoc());
  return FunctionTypeLoc();
}

bool isKAndRFunctionDecl(FunctionDecl *FD) {
  return !FD->hasPrototype() && FD->getNumParams();
}

namespace {

// See clang/docs/checkedc/3C/clang-tidy.md#_3c-name-prefix
// NOLINTNEXTLINE(readability-identifier-naming)
class _3CFormatStringHandler
    : public analyze_format_string::FormatStringHandler {
  unsigned DataStartIdx;
  std::set<unsigned> &StringArgIndices;

public:
  _3CFormatStringHandler(unsigned DataStartIdx,
                         std::set<unsigned> &StringArgIndices)
      : DataStartIdx(DataStartIdx), StringArgIndices(StringArgIndices) {}
  bool HandlePrintfSpecifier(const analyze_printf::PrintfSpecifier &FS,
                             const char *StartSpecifier,
                             unsigned SpecifierLen) override {
    if (FS.consumesDataArgument() &&
        FS.getConversionSpecifier().getKind() ==
            analyze_printf::PrintfConversionSpecifier::sArg)
      StringArgIndices.insert(DataStartIdx + FS.getArgIndex());
    return true;
  }
};

} // namespace

// Unfortunately, this duplicates some logic from different parts of
// SemaChecking.cpp. We handle only the common case. If we get this wrong, it's
// not a big deal: 3C may just infer some checked pointer types incorrectly.
void getPrintfStringArgIndices(const CallExpr *CE, const FunctionDecl *Callee,
                               const clang::ASTContext &Context,
                               std::set<unsigned> &StringArgIndices) {
  for (const FormatAttr *Attr : Callee->specific_attrs<FormatAttr>()) {
    if (Sema::GetFormatStringType(Attr) != Sema::FST_Printf)
      continue;
    if (Attr->getFirstArg() == 0)
      // This means the data arguments are not available to check.
      continue;
    unsigned FormatIdx = Attr->getFormatIdx() - 1;
    unsigned DataStartIdx = Attr->getFirstArg() - 1;
    if (FormatIdx >= CE->getNumArgs())
      continue;
    const Expr *FormatExpr =
        CE->getArg(FormatIdx)->IgnoreImpCasts()->IgnoreExprTmp();
    const clang::StringLiteral *FormatLiteral =
        dyn_cast<clang::StringLiteral>(FormatExpr);
    if (!FormatLiteral || FormatLiteral->getCharByteWidth() != 1)
      continue;
    StringRef Str = FormatLiteral->getString();
    _3CFormatStringHandler Handler(DataStartIdx, StringArgIndices);
    analyze_format_string::ParsePrintfString(
        Handler, Str.data(), Str.data() + Str.size(), Context.getLangOpts(),
        Context.getTargetInfo(), false);
  }
}

int64_t getStmtIdWorkaround(const Stmt *St, const ASTContext &Context) {
  // Stmt::getID uses Context.getAllocator().identifyKnownAlignedObject(St) to
  // derive a unique ID for St from its pointer in a way that should be
  // reproducible if the exact same source file is loaded in the exact same
  // environment, since if AST building is deterministic, the sequence of memory
  // allocations should be identical.
  // Context.getAllocator().identifyKnownObject(St) generates an ID that is
  // normally the offset of the Stmt object within the allocator's memory slabs,
  // so it is divisible by alignof(Stmt). identifyKnownAlignedObject wraps
  // identifyKnownObject to divide the ID by alignof(Stmt) (after asserting that
  // it is divisible), probably in an effort to produce smaller IDs to make some
  // data structures more efficient. However, if the Stmt is allocated in a
  // custom-size slab because it exceeds the default slab size of 4096, then
  // identifyKnownObject returns -1 minus the offset, which is _not_ divisible
  // by alignof(Stmt) because of the -1. Unfortunately,
  // identifyKnownAlignedObject doesn't account for this rare case, and its
  // assertion fails (https://bugs.llvm.org/show_bug.cgi?id=49926).
  //
  // Since 3C doesn't currently use the ID in a data structure that benefits
  // from smaller IDs, we may as well just use identifyKnownObject. If we wanted
  // smaller IDs, the solution would probably be to have
  // identifyKnownAlignedObject fix the alignment of negative IDs by subtracting
  // (alignof(Stmt) - 1) before dividing.
  return Context.getAllocator().identifyKnownObject(St);
}

// Get the SourceLocation for the end of any Checked C bounds or interop type
// annotations on a declaration. Returns an invalid source location if no
// Checked C annotations are present.
SourceLocation getCheckedCAnnotationsEnd(const Decl *D) {
  SourceManager &SM = D->getASTContext().getSourceManager();
  SourceLocation End;

  // Update the current end SourceLocation to the new SourceLocation if the new
  // location is valid and comes after the current end location.
  auto UpdateEnd = [&SM, &End](SourceLocation SL) {
    if (SL.isValid() &&
        (!End.isValid() || SM.isBeforeInTranslationUnit(End, SL)))
      End = SL;
  };

  if (auto *DD = dyn_cast<DeclaratorDecl>(D)) {
    if (auto *InteropE = DD->getInteropTypeExpr())
      UpdateEnd(InteropE->getEndLoc());
    if (auto *BoundsE = DD->getBoundsExpr())
      UpdateEnd(BoundsE->getEndLoc());
  }

  return End;
}

SourceRange getDeclSourceRangeWithAnnotations(const clang::Decl *D,
                                              bool IncludeInitializer) {
  SourceManager &SM = D->getASTContext().getSourceManager();
  SourceRange SR;
  const VarDecl *VD;
  // Only a VarDecl can have an initializer. VarDecl's implementation of the
  // getSourceRange virtual method includes the initializer, but we can manually
  // call DeclaratorDecl's implementation, which excludes the initializer.
  if (!IncludeInitializer && (VD = dyn_cast<VarDecl>(D)) != nullptr)
    SR = VD->DeclaratorDecl::getSourceRange();
  else
    SR = D->getSourceRange();
  if (!SR.isValid())
    return SR;
  SourceLocation DeclEnd = SR.getEnd();

  // Partial workaround for a compiler bug where if D has certain checked
  // pointer types such as `_Ptr<int *(void)>` (seen in the partial_checked.c
  // regression test), D->getSourceRange() returns only the _Ptr token. (As of
  // this writing on 2021-11-18, no bug report has been filed against the
  // compiler, but https://github.com/correctcomputation/checkedc-clang/pull/723
  // tracks our work on the bug.)
  //
  // Always extend the range at least through the name (given by
  // D->getLocation()). That fixes the `_Ptr<int *(void)> x` case but not cases
  // with additional syntax after the name, such as `_Ptr<int *(void)> x[10]`.
  SourceLocation DeclLoc = D->getLocation();
  if (SM.isBeforeInTranslationUnit(DeclEnd, DeclLoc))
    DeclEnd = DeclLoc;

  SourceLocation AnnotationsEnd = getCheckedCAnnotationsEnd(D);
  if (AnnotationsEnd.isValid() &&
      SM.isBeforeInTranslationUnit(DeclEnd, AnnotationsEnd))
    DeclEnd = AnnotationsEnd;

  SR.setEnd(DeclEnd);
  return SR;
}
