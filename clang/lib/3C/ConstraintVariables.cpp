//=--ConstraintVariables.cpp--------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of ConstraintVariables methods.
//
//===----------------------------------------------------------------------===//

#include "clang/3C/ConstraintVariables.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ProgramInfo.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/CommandLine.h"
#include <sstream>

using namespace clang;
// Macro for boolean implication.
#define IMPLIES(a, b) ((a) ? (b) : true)

static llvm::cl::OptionCategory OptimizationCategory("Optimization category");
static llvm::cl::opt<bool>
    DisableRDs("disable-rds",
               llvm::cl::desc("Disable reverse edges for Checked Constraints."),
               llvm::cl::init(false), llvm::cl::cat(OptimizationCategory));

static llvm::cl::opt<bool>
    DisableFunctionEdges("disable-fnedgs",
                         llvm::cl::desc("Disable reverse edges for external functions."),
                         llvm::cl::init(false), llvm::cl::cat(OptimizationCategory));

std::string ConstraintVariable::getRewritableOriginalTy() const {
  std::string OrigTyString = getOriginalTy();
  std::string SpaceStr = " ";
  std::string AsterixStr = "*";
  // If the type does not end with " " or *
  // we need to add space.
  if (!std::equal(SpaceStr.rbegin(), SpaceStr.rend(), OrigTyString.rbegin()) &&
      !std::equal(AsterixStr.rbegin(), AsterixStr.rend(),
                  OrigTyString.rbegin())) {
    OrigTyString += " ";
  }
  return OrigTyString;
}

PointerVariableConstraint *PointerVariableConstraint::getWildPVConstraint(
    Constraints &CS, const std::string &Rsn, PersistentSourceLoc *PSL) {
  VarAtom *VA = CS.createFreshGEQ("wildvar", VarAtom::V_Other, CS.getWild(),
                                  Rsn, PSL);
  return new PointerVariableConstraint({VA}, {CS.getWild()}, "unsigned",
                                       "wildvar", nullptr, "");
}

PointerVariableConstraint *
PointerVariableConstraint::getNonPtrPVConstraint(Constraints &CS) {
  static PointerVariableConstraint *GlobalNonPtrPV = nullptr;
  if (GlobalNonPtrPV == nullptr) {
    return new PointerVariableConstraint({}, {}, "unsigned", "basevar", nullptr,
                                         "");
  }
  return GlobalNonPtrPV;
}

PointerVariableConstraint *
PointerVariableConstraint::getNamedNonPtrPVConstraint(StringRef Name,
                                                      Constraints &CS) {
  return new PointerVariableConstraint({}, {}, "unsigned", std::string(Name),
                                       nullptr, "");
}

PointerVariableConstraint *
PointerVariableConstraint::derefPVConstraint(PointerVariableConstraint *PVC) {
  std::vector<Atom *> Vars = PVC->Vars;
  std::vector<ConstAtom *> SrcVars = PVC->SrcVars;
  assert(!PVC->Vars.empty() && !SrcVars.empty());
  Vars.erase(Vars.begin());
  SrcVars.erase(SrcVars.begin());
  return new PointerVariableConstraint(Vars, SrcVars, PVC->getTy(),
                                       PVC->getName(), PVC->getFV(),
                                       PVC->getItype());
}

PointerVariableConstraint *
PointerVariableConstraint::addAtomPVConstraint(PointerVariableConstraint *PVC,
                                               ConstAtom *PtrTyp,
                                               Constraints &CS) {
  VarAtom *NewA = CS.getFreshVar("&" + PVC->Name, VarAtom::V_Other);
  CS.addConstraint(CS.createGeq(NewA, PtrTyp, false));
  std::vector<Atom*> Vars = PVC->Vars;
  std::vector<ConstAtom*> SrcVars = PVC->SrcVars;
  if (!Vars.empty()) {
    if (auto *VA = dyn_cast<VarAtom>(*Vars.begin())) {
      // If PVC is already a pointer, add implication forcing outermost one to be
      // wild if this added one is.
      auto *Prem = CS.createGeq(NewA, CS.getWild());
      auto *Conc = CS.createGeq(VA, CS.getWild());
      CS.addConstraint(CS.createImplies(Prem, Conc));
    }
  }

  Vars.insert(Vars.begin(), NewA);
  SrcVars.insert(SrcVars.begin(), PtrTyp);
  return new PointerVariableConstraint(Vars, SrcVars, PVC->BaseType, PVC->Name,
                                       PVC->FV, PVC->ItypeStr);
}

PointerVariableConstraint::PointerVariableConstraint(
    PointerVariableConstraint *Ot, Constraints &CS)
    : ConstraintVariable(ConstraintVariable::PointerVariable, Ot->OriginalType,
                         Ot->Name),
      FV(nullptr), PartOfFuncPrototype(Ot->PartOfFuncPrototype) {
  this->ArrSizes = Ot->ArrSizes;
  this->ArrSizeStrs = Ot->ArrSizeStrs;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  this->ValidBoundsKey = Ot->ValidBoundsKey;
  this->BKey = Ot->BKey;

  assert(Ot->Vars.size() == Ot->SrcVars.size());
  auto VAIt = Ot->Vars.begin();
  auto CAIt = Ot->SrcVars.begin();
  while (VAIt != Ot->Vars.end() && CAIt != Ot->SrcVars.end()) {
    if (ConstAtom *CA = dyn_cast<ConstAtom>(*VAIt)) {
      this->Vars.push_back(CA);
      this->SrcVars.push_back(CA);
    } else if (VarAtom *VA = dyn_cast<VarAtom>(*VAIt)) {
      VarAtom *FreshVA = CS.getFreshVar(VA->getName(), VA->getVarKind());
      this->Vars.push_back(FreshVA);
      this->SrcVars.push_back(*CAIt);
      if (!isa<WildAtom>(*CAIt))
        CS.addConstraint(CS.createGeq(*CAIt, FreshVA, false));
    }
    ++VAIt;
    ++CAIt;
  }

  if (Ot->FV != nullptr) {
    this->FV = dyn_cast<FVConstraint>(Ot->FV->getCopy(CS));
  }
  this->Parent = Ot;
  this->GenericIndex = Ot->GenericIndex;
  this->IsZeroWidthArray = Ot->IsZeroWidthArray;
  this->BaseType = Ot->BaseType;
  this->SrcHasItype = Ot->SrcHasItype;
  this->IsVoidPtr = Ot->IsVoidPtr;
  this->TypedefLevelInfo = Ot->TypedefLevelInfo;
}

PointerVariableConstraint::PointerVariableConstraint(DeclaratorDecl *D,
                                                     ProgramInfo &I,
                                                     const ASTContext &C)
  : PointerVariableConstraint(D->getType(), D, std::string(D->getName()), I, C,
                              nullptr, -1, false, D->getTypeSourceInfo()) {}

PointerVariableConstraint::PointerVariableConstraint(TypedefDecl *D,
                                                     ProgramInfo &I,
                                                     const ASTContext &C)
  : PointerVariableConstraint(D->getUnderlyingType(), nullptr,
                              D->getNameAsString(), I, C, nullptr, -1,
                              false, D->getTypeSourceInfo()) {}

PointerVariableConstraint::PointerVariableConstraint(Expr *E, ProgramInfo &I,
                                                     const ASTContext &C)
  : PointerVariableConstraint(E->getType(), nullptr, E->getStmtClassName(), I,
                              C, nullptr) {}

// Simple recursive visitor for determining if a type contains a typedef
// entrypoint is find().
class TypedefLevelFinder : public RecursiveASTVisitor<TypedefLevelFinder> {
public:
  static struct InternalTypedefInfo find(const QualType &QT) {
    TypedefLevelFinder TLF;
    QualType ToSearch;
    // If the type is currently a typedef, desugar that.
    // This is so we can see if the type _contains_ a typedef.
    if (const auto *TDT = dyn_cast<TypedefType>(QT))
      ToSearch = TDT->desugar();
    else
      ToSearch = QT;
    TLF.TraverseType(ToSearch);
    // If we found a typedef then we need to have filled out the name field.
    assert(IMPLIES(TLF.HasTypedef, TLF.TDname != ""));
    struct InternalTypedefInfo Info = {TLF.HasTypedef, TLF.TypedefLevel,
                                       TLF.TDname};
    return Info;
  }

  bool VisitTypedefType(TypedefType *TT) {
    HasTypedef = true;
    auto *TDT = TT->getDecl();
    TDname = TDT->getNameAsString();
    return false;
  }

  bool VisitPointerType(PointerType *PT) {
    TypedefLevel++;
    return true;
  }

  bool VisitArrayType(ArrayType *AT) {
    TypedefLevel++;
    return true;
  }

private:
  int TypedefLevel = 0;
  std::string TDname = "";
  bool HasTypedef = false;
};

PointerVariableConstraint::PointerVariableConstraint(
    const QualType &QT, DeclaratorDecl *D, std::string N, ProgramInfo &I,
    const ASTContext &C, std::string *InFunc, int ForceGenericIndex,
    bool VarAtomForChecked, TypeSourceInfo *TSInfo, const QualType &ITypeT)
    : ConstraintVariable(ConstraintVariable::PointerVariable,
                         tyToStr(QT.getTypePtr()), N),
      FV(nullptr), SrcHasItype(false), PartOfFuncPrototype(InFunc != nullptr),
      Parent(nullptr) {
  QualType QTy = QT;
  const Type *Ty = QTy.getTypePtr();
  auto &CS = I.getConstraints();
  // If the type is a decayed type, then maybe this is the result of
  // decaying an array to a pointer. If the original type is some
  // kind of array type, we want to use that instead.
  if (const DecayedType *DC = dyn_cast<DecayedType>(Ty)) {
    QualType QTytmp = DC->getOriginalType();
    if (QTytmp->isArrayType() || QTytmp->isIncompleteArrayType()) {
      QTy = QTytmp;
      Ty = QTy.getTypePtr();
    }
  }

  bool IsTypedef = false;
  if (Ty->getAs<TypedefType>())
    IsTypedef = true;

  bool IsDeclTy = false;

  auto &ABInfo = I.getABoundsInfo();
  if (D != nullptr) {
    if (ABInfo.tryGetVariable(D, BKey)) {
      ValidBoundsKey = true;
    }
    if (D->hasBoundsAnnotations()) {
      BoundsAnnotations BA = D->getBoundsAnnotations();
      BoundsExpr *BExpr = BA.getBoundsExpr();
      if (BExpr != nullptr) {
        SourceRange R = BExpr->getSourceRange();
        if (R.isValid()) {
          BoundsAnnotationStr = getSourceText(R, C);
        }
        if (D->hasBoundsAnnotations() && ABInfo.isValidBoundVariable(D)) {
          assert(ABInfo.tryGetVariable(D, BKey) &&
                 "Is expected to have valid Bounds key");

          ABounds *NewB = ABounds::getBoundsInfo(&ABInfo, BExpr, C);
          ABInfo.insertDeclaredBounds(D, NewB);
        }
      }
    }

    IsDeclTy = D->getType() == QT; // If false, then QT may be D's return type
    if (InteropTypeExpr *ITE = D->getInteropTypeExpr()) {
      // External variables can also have itype.
      // Check if the provided declaration is an external
      // variable.
      // For functions, check to see that if we are analyzing
      // function return types.
      bool AnalyzeITypeExpr = IsDeclTy;
      if (!AnalyzeITypeExpr) {
        const Type *OrigType = Ty;
        if (isa<FunctionDecl>(D)) {
          FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
          OrigType = FD->getType().getTypePtr();
        }
        if (OrigType->isFunctionProtoType()) {
          const FunctionProtoType *FPT = OrigType->getAs<FunctionProtoType>();
          AnalyzeITypeExpr = (FPT->getReturnType() == QT);
        }
      }
      if (AnalyzeITypeExpr) {
        QualType InteropType = ITE->getTypeAsWritten();
        QTy = InteropType;
        Ty = QTy.getTypePtr();
        SrcHasItype = true;

        SourceRange R = ITE->getSourceRange();
        if (R.isValid()) {
          ItypeStr = getSourceText(R, C);
          assert(ItypeStr.size() > 0);
        }
      }
    }
  }
  if (!SrcHasItype && !ITypeT.isNull()) {
    QTy = ITypeT;
    Ty = QTy.getTypePtr();
    SrcHasItype = true;
  }

  // At this point `QTy`/`Ty` hold the computed type (and `QT` still holds the
  // input type). It will be consumed to create atoms, so any code that needs
  // to be coordinated with the atoms should access it here first.

  TypedefLevelInfo = TypedefLevelFinder::find(QTy);

  if (ForceGenericIndex >= 0) {
    GenericIndex = ForceGenericIndex;
  } else {
    GenericIndex = -1;
    // This makes a lot of assumptions about how the AST will look, and limits
    // it to one level.
    // TODO: Enhance TypedefLevelFinder to get this info.
    if (Ty->isPointerType()) {
      auto *PtrTy = Ty->getPointeeType().getTypePtr();
      if (auto *TypdefTy = dyn_cast_or_null<TypedefType>(PtrTy)) {
        const auto *Tv = dyn_cast<TypeVariableType>(TypdefTy->desugar());
        if (Tv)
          GenericIndex = Tv->GetIndex();
      }
    }
  }

  bool VarCreated = false;
  bool IsArr = false;
  bool IsIncompleteArr = false;
  bool IsTopMost = true;
  uint32_t TypeIdx = 0;
  std::string Npre = InFunc ? ((*InFunc) + ":") : "";
  VarAtom::VarKind VK =
      InFunc ? (N == RETVAR ? VarAtom::V_Return : VarAtom::V_Param)
             : VarAtom::V_Other;

  // Even though references don't exist in C, `va_list` is a typedef of
  // `__builtin_va_list &` on windows. In order to generate correct constraints
  // for var arg functions on windows, we need to strip the reference type.
  if (Ty->isLValueReferenceType()) {
    QTy = Ty->getPointeeType();
    Ty = QTy.getTypePtr();
  }

  IsZeroWidthArray = false;

  TypeLoc TLoc = TypeLoc();
  if (D && D->getTypeSourceInfo())
    TLoc = D->getTypeSourceInfo()->getTypeLoc();

  while (Ty->isPointerType() || Ty->isArrayType()) {
    // Is this a VarArg type?
    std::string TyName = tyToStr(Ty);
    if (isVarArgType(TyName)) {
      // Variable number of arguments. Make it WILD.
      std::string Rsn = "Variable number of arguments.";
      VarAtom *WildVA = CS.createFreshGEQ(Npre + N, VK, CS.getWild(), Rsn);
      Vars.push_back(WildVA);
      SrcVars.push_back(CS.getWild());
      VarCreated = true;
      break;
    }

    if (Ty->isCheckedPointerType() || Ty->isCheckedArrayType()) {
      ConstAtom *CAtom = nullptr;
      if (Ty->isCheckedPointerNtArrayType() || Ty->isNtCheckedArrayType()) {
        // This is an NT array type.
        CAtom = CS.getNTArr();
      } else if (Ty->isCheckedPointerArrayType() || Ty->isCheckedArrayType()) {
        // This is an array type.
        CAtom = CS.getArr();

        // In CheckedC, a pointer can be freely converted to a size 0 array pointer,
        // but our constraint system does not allow this. To enable converting calls
        // to functions with types similar to free, size 0 array pointers are made PTR
        // instead of ARR.
        if (D && D->hasBoundsExpr())
          if (BoundsExpr *BE = D->getBoundsExpr())
            if (isZeroBoundsExpr(BE, C)) {
              IsZeroWidthArray = true;
              CAtom = CS.getPtr();
            }

      } else if (Ty->isCheckedPointerPtrType()) {
        // This is a regular checked pointer.
        CAtom = CS.getPtr();
      }
      VarCreated = true;
      assert(CAtom != nullptr && "Unable to find the type "
                                 "of the checked pointer.");
      Atom *NewAtom;
      if (VarAtomForChecked)
        NewAtom = CS.getFreshVar(Npre + N, VK);
      else
        NewAtom = CAtom;
      Vars.push_back(NewAtom);
      SrcVars.push_back(CAtom);
    }

    if (Ty->isArrayType() || Ty->isIncompleteArrayType()) {
      IsArr = true;
      IsIncompleteArr = Ty->isIncompleteArrayType();

      // Boil off the typedefs in the array case.
      // TODO this will need to change to properly account for typedefs
      bool Boiling = true;
      while (Boiling) {
        if (const TypedefType *TydTy = dyn_cast<TypedefType>(Ty)) {
          QTy = TydTy->desugar();
          Ty = QTy.getTypePtr();
          if (!TLoc.isNull()) {
            auto TDefTLoc = TLoc.getAs<TypedefTypeLoc>();
            if (!TDefTLoc.isNull())
              TLoc = TDefTLoc.getNextTypeLoc();
          }
        } else if (const ParenType *ParenTy = dyn_cast<ParenType>(Ty)) {
          QTy = ParenTy->desugar();
          Ty = QTy.getTypePtr();
          if (!TLoc.isNull()) {
            auto ParenTLoc = TLoc.getAs<ParenTypeLoc>();
            if (!ParenTLoc.isNull())
              TLoc = ParenTLoc.getInnerLoc();
          }
        } else {
          Boiling = false;
        }
      }

      // See if there is a constant size to this array type at this position.
      if (const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(Ty)) {
        ArrSizes[TypeIdx] = std::pair<OriginalArrType, uint64_t>(
            O_SizedArray, CAT->getSize().getZExtValue());

        if (!TLoc.isNull()) {
          auto ArrTLoc = TLoc.getAs<ArrayTypeLoc>();
          if (!ArrTLoc.isNull()) {
            std::string SizeStr = getSourceText(ArrTLoc.getBracketsRange(), C);
            if (!SizeStr.empty())
              ArrSizeStrs[TypeIdx] = SizeStr;
          }
        }

        // If this is the top-most pointer variable?
        if (hasBoundsKey() && IsTopMost) {
          BoundsKey CBKey = ABInfo.getConstKey(CAT->getSize().getZExtValue());
          ABounds *NB = new CountBound(CBKey);
          ABInfo.insertDeclaredBounds(D, NB);
        }
      } else {
        ArrSizes[TypeIdx] =
            std::pair<OriginalArrType, uint64_t>(O_UnSizedArray, 0);
      }

      // Iterate.
      if (const ArrayType *ArrTy = dyn_cast<ArrayType>(Ty)) {
        QTy = ArrTy->getElementType();
        Ty = QTy.getTypePtr();
      } else {
        llvm_unreachable("unknown array type");
      }
    } else {

      // Save here if QTy is qualified or not into a map that
      // indexes K to the qualification of QTy, if any.
      insertQualType(TypeIdx, QTy);

      ArrSizes[TypeIdx] = std::pair<OriginalArrType, uint64_t>(O_Pointer, 0);

      // Iterate.
      QTy = QTy.getSingleStepDesugaredType(C);
      QTy = QTy.getTypePtr()->getPointeeType();
      Ty = QTy.getTypePtr();
    }

    // This type is not a constant atom. We need to create a VarAtom for this.

    if (!VarCreated) {
      VarAtom *VA = CS.getFreshVar(Npre + N, VK);
      Vars.push_back(VA);
      SrcVars.push_back(CS.getWild());

      // Incomplete arrays are lower bounded to ARR because the transformation
      // int[] -> _Ptr<int> is permitted while int[1] -> _Ptr<int> is not.
      if (IsIncompleteArr)
        CS.addConstraint(CS.createGeq(VA, CS.getArr(), false));
      else if (IsArr)
        CS.addConstraint(CS.createGeq(CS.getArr(), VA, false));
    }

    // Prepare for next level of pointer
    VarCreated = false;
    IsArr = false;
    TypeIdx++;
    Npre = Npre + "*";
    VK = VarAtom::
        V_Other; // only the outermost pointer considered a param/return
    IsTopMost = false;
    if (!TLoc.isNull())
      TLoc = TLoc.getNextTypeLoc();
  }
  insertQualType(TypeIdx, QTy);

  // If, after boiling off the pointer-ness from this type, we hit a
  // function, then create a base-level FVConstraint that we carry
  // around too.
  if (Ty->isFunctionType())
    // C function-pointer type declarator syntax embeds the variable
    // name within the function-like syntax. For example:
    //    void (*fname)(int, int) = ...;
    // If a typedef'ed type name is used, the name can be omitted
    // because it is not embedded like that. Instead, it has the form
    //    tn fname = ...,
    // where tn is the typedef'ed type name.
    // There is possibly something more elegant to do in the code here.
    FV = new FVConstraint(Ty, IsDeclTy ? D : nullptr, IsTypedef ? "" : N, I, C,
                          TSInfo);

  // Get a string representing the type without pointer and array indirection.
  BaseType = extractBaseType(D, TSInfo, QT, Ty, C);

  IsVoidPtr = isTypeHasVoid(QT);
  bool IsWild = !getIsGeneric() && (isVarArgType(BaseType) || IsVoidPtr);
  if (IsWild) {
    std::string Rsn =
        IsVoidPtr ? "Default void* type" : "Default Var arg list type";
    // TODO: Github issue #61: improve handling of types for variable arguments.
    for (const auto &V : Vars)
      if (VarAtom *VA = dyn_cast<VarAtom>(V))
        CS.addConstraint(CS.createGeq(VA, CS.getWild(), Rsn));
  }

  // Add qualifiers.
  std::ostringstream QualStr;
  getQualString(TypeIdx, QualStr);
  BaseType = QualStr.str() + BaseType;

  // Here lets add implication that if outer pointer is WILD
  // then make the inner pointers WILD too.
  if (Vars.size() > 1) {
    bool UsedPrGeq = false;
    for (auto VI = Vars.begin(), VE = Vars.end(); VI != VE; VI++) {
      if (VarAtom *VIVar = dyn_cast<VarAtom>(*VI)) {
        // Premise.
        Geq *PrGeq = new Geq(VIVar, CS.getWild());
        UsedPrGeq = false;
        for (auto VJ = (VI + 1); VJ != VE; VJ++) {
          if (VarAtom *VJVar = dyn_cast<VarAtom>(*VJ)) {
            // Conclusion.
            Geq *CoGeq = new Geq(VJVar, CS.getWild());
            CS.addConstraint(CS.createImplies(PrGeq, CoGeq));
            UsedPrGeq = true;
          }
        }
        // Delete unused constraint.
        if (!UsedPrGeq) {
          delete (PrGeq);
        }
      }
    }
  }
}

std::string PointerVariableConstraint::tryExtractBaseType(DeclaratorDecl *D,
                                                          TypeSourceInfo *TSI,
                                                          QualType QT,
                                                          const Type *Ty,
                                                          const ASTContext &C) {
  bool FoundBaseTypeInSrc = false;
  if (D && !TSI)
    TSI = D->getTypeSourceInfo();
  if (!QT->isOrContainsCheckedType() && !Ty->getAs<TypedefType>() && D && TSI) {
    // Try to extract the type from original source to preserve defines
    TypeLoc TL = TSI->getTypeLoc();
    if (isa<FunctionDecl>(D)) {
      FoundBaseTypeInSrc = D->getAsFunction()->getReturnType() == QT;
      TL = getBaseTypeLoc(TL).getAs<FunctionTypeLoc>();
      // FunctionDecl that doesn't have function type? weird
      if (TL.isNull())
        return "";
      TL = TL.getAs<clang::FunctionTypeLoc>().getReturnLoc();
    } else {
      FoundBaseTypeInSrc = D->getType() == QT;
    }
    if (!TL.isNull()) {
      TypeLoc BaseLoc = getBaseTypeLoc(TL);
      // Only proceed if the base type location is not null, amd it is not a
      // typedef type location.
      if (!BaseLoc.isNull() && BaseLoc.getAs<TypedefTypeLoc>().isNull()) {
        SourceRange SR = BaseLoc.getSourceRange();
        if (FoundBaseTypeInSrc && SR.isValid())
          return getSourceText(SR, C);
      }
    }
  }

  return "";
}

std::string PointerVariableConstraint::extractBaseType(DeclaratorDecl *D,
                                                       TypeSourceInfo *TSI,
                                                       QualType QT,
                                                       const Type *Ty,
                                                       const ASTContext &C) {
  std::string BaseTypeStr = tryExtractBaseType(D, TSI, QT, Ty, C);
  // Fall back to rebuilding the base type based on type passed to constructor
  if (BaseTypeStr.empty())
    BaseTypeStr = tyToStr(Ty);

  return BaseTypeStr;
}

void PointerVariableConstraint::print(raw_ostream &O) const {
  O << "{ ";
  for (const auto &I : Vars) {
    I->print(O);
    O << " ";
  }
  O << " }";

  if (FV) {
    O << "(";
    FV->print(O);
    O << ")";
  }
}

void PointerVariableConstraint::dumpJson(llvm::raw_ostream &O) const {
  O << "{\"PointerVar\":{";
  O << "\"Vars\":[";
  bool AddComma = false;
  for (const auto &I : Vars) {
    if (AddComma) {
      O << ",";
    }
    I->dumpJson(O);

    AddComma = true;
  }
  O << "], \"name\":\"" << getName() << "\"";
  if (FV) {
    O << ", \"FunctionVariable\":";
    FV->dumpJson(O);
  }
  O << "}}";
}

void PointerVariableConstraint::getQualString(uint32_t TypeIdx,
                                              std::ostringstream &Ss) const {
  auto QIter = QualMap.find(TypeIdx);
  if (QIter != QualMap.end()) {
    for (Qualification Q : QIter->second) {
      switch (Q) {
      case ConstQualification:
        Ss << "const ";
        break;
      case VolatileQualification:
        Ss << "volatile ";
        break;
      case RestrictQualification:
        Ss << "restrict ";
        break;
      }
    }
  }
}

void PointerVariableConstraint::insertQualType(uint32_t TypeIdx,
                                               QualType &QTy) {
  if (QTy.isConstQualified())
    QualMap[TypeIdx].insert(ConstQualification);
  if (QTy.isVolatileQualified())
    QualMap[TypeIdx].insert(VolatileQualification);
  if (QTy.isRestrictQualified())
    QualMap[TypeIdx].insert(RestrictQualification);
}

// Take an array or nt_array variable, determines if it is a constant array,
// and if so emits the appropriate syntax for a stack-based array.
bool PointerVariableConstraint::emitArraySize(
  std::stack<std::string> &ConstSizeArrs, uint32_t TypeIdx,
  Atom::AtomKind Kind) const {
  auto I = ArrSizes.find(TypeIdx);
  assert(I != ArrSizes.end());
  OriginalArrType Oat = I->second.first;
  uint64_t Oas = I->second.second;

  if (Oat == O_SizedArray) {
    std::ostringstream SizeStr;
    if (Kind != Atom::A_Wild)
      SizeStr << (Kind == Atom::A_NTArr ? " _Nt_checked" : " _Checked");
    if (ArrSizeStrs.find(TypeIdx) != ArrSizeStrs.end()) {
      std::string SrcSizeStr = ArrSizeStrs.find(TypeIdx)->second;
      assert(!SrcSizeStr.empty());
      // In some weird edge cases the size of the array is defined by a macro
      // where the macro also includes the brackets. We need to add a space
      // between the _Checked annotation and this macro to ensure they aren't
      // concatenated into a single token.
      if (SrcSizeStr[0] != '[')
        SizeStr << " ";
      SizeStr << SrcSizeStr;
    } else
      SizeStr << "[" << Oas << "]";
    ConstSizeArrs.push(SizeStr.str());
    return true;
  }
  return false;
}

/*  addArrayAnnotiations
 *  This function takes all the stacked annotations for constant arrays
 *  and pops them onto the EndStrs, this ensures the right order of annotations
 *   */
void PointerVariableConstraint::addArrayAnnotations(
    std::stack<std::string> &ConstArrs,
    std::deque<std::string> &EndStrs) const {
  while (!ConstArrs.empty()) {
    auto NextStr = ConstArrs.top();
    ConstArrs.pop();
    EndStrs.push_front(NextStr);
  }
  assert(ConstArrs.empty());
}

bool PointerVariableConstraint::isTypedef(void) { return IsTypedef; }

void PointerVariableConstraint::setTypedef(TypedefNameDecl *T, std::string S) {
  IsTypedef = true;
  TDT = T;
  TypedefString = S;
}

// Mesh resolved constraints with the PointerVariableConstraints set of
// variables and potentially nested function pointer declaration. Produces a
// string that can be replaced in the source code.

std::string PointerVariableConstraint::gatherQualStrings(void) const {
  std::ostringstream S;
  getQualString(0, S);
  return S.str();
}

std::string PointerVariableConstraint::mkString(Constraints &CS,
                                                bool EmitName, bool ForItype,
                                                bool EmitPointee,
                                                bool UnmaskTypedef,
                                                std::string UseName) const {

  // The name field encodes if this variable is the return type for a function.
  // TODO: store this information in a separate field.
  bool IsReturn = getName() == RETVAR;

  if (UseName.empty())
    UseName = getName();

  if (IsTypedef && !UnmaskTypedef) {
    return gatherQualStrings() + TypedefString +
           (EmitName && !IsReturn ? (" " + UseName) : " ");
  }

  std::ostringstream Ss;
  // Annotations that will need to be placed on the identifier of an unchecked
  // function pointer.
  std::ostringstream FptrInner;
  // This deque will store all the type strings that need to pushed
  // to the end of the type string. This is typically things like
  // closing delimiters.
  std::deque<std::string> EndStrs;
  // This will store stacked array decls to ensure correct order
  // We encounter constant arrays variables in the reverse order they
  // need to appear in, so the LIFO structure reverses these annotations
  std::stack<std::string> ConstArrs;
  // Have we emitted the string for the base type
  bool EmittedBase = false;
  // Have we emitted the name of the variable yet?
  bool EmittedName = false;
  // Was the last variable an Array?
  bool PrevArr = false;
  // Is the entire type so far an array?
  bool AllArrays = true;
  if (!EmitName || IsReturn)
    EmittedName = true;
  uint32_t TypeIdx = 0;

  auto It = Vars.begin();
  auto I = 0;
  // Skip over first pointer level if only emitting pointee string.
  // This is needed when inserting type arguments.
  if (EmitPointee)
    ++It;
  // Iterate through the vars(), but if we have an internal typedef, then stop
  // once you reach the typedef's level.
  for (; It != Vars.end() && IMPLIES(TypedefLevelInfo.HasTypedef,
                                     I < TypedefLevelInfo.TypedefLevel);
       ++It, I++) {
    const auto &V = *It;
    ConstAtom *C = nullptr;
    if (ConstAtom *CA = dyn_cast<ConstAtom>(V)) {
      C = CA;
    } else {
      VarAtom *VA = dyn_cast<VarAtom>(V);
      assert(VA != nullptr && "Constraint variable can "
                              "be either constant or VarAtom.");
      C = CS.getVariables().at(VA).first;
    }
    assert(C != nullptr);

    Atom::AtomKind K = C->getKind();

    // If this is not an itype
    // make this wild as it can hold any pointer type.
    if (!ForItype && BaseType == "void")
      K = Atom::A_Wild;

    if (PrevArr && ArrSizes.at(TypeIdx).first != O_SizedArray && !EmittedName) {
      EmittedName = true;
      addArrayAnnotations(ConstArrs, EndStrs);
      EndStrs.push_front(" " + UseName);
    }
    PrevArr = ArrSizes.at(TypeIdx).first == O_SizedArray;

    switch (K) {
    case Atom::A_Ptr:
      getQualString(TypeIdx, Ss);

      // We need to check and see if this level of variable
      // is constrained by a bounds safe interface. If it is,
      // then we shouldn't re-write it.
      AllArrays = false;
      EmittedBase = false;
      Ss << "_Ptr<";
      EndStrs.push_front(">");
      break;
    case Atom::A_Arr:
      // If this is an array.
      getQualString(TypeIdx, Ss);
      // If it's an Arr, then the character we substitute should
      // be [] instead of *, IF, the original type was an array.
      // And, if the original type was a sized array of size K.
      // we should substitute [K].
      if (emitArraySize(ConstArrs, TypeIdx, K))
        break;
      AllArrays = false;
      // We need to check and see if this level of variable
      // is constrained by a bounds safe interface. If it is,
      // then we shouldn't re-write it.
      EmittedBase = false;
      Ss << "_Array_ptr<";
      EndStrs.push_front(">");
      break;
    case Atom::A_NTArr:
      if (emitArraySize(ConstArrs, TypeIdx, K))
        break;
      AllArrays = false;
      // This additional check is to prevent fall-through from the array.
      if (K == Atom::A_NTArr) {
        // If this is an NTArray.
        getQualString(TypeIdx, Ss);

        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        EmittedBase = false;
        Ss << "_Nt_array_ptr<";
        EndStrs.push_front(">");
        break;
      }
      LLVM_FALLTHROUGH;
    // If there is no array in the original program, then we fall through to
    // the case where we write a pointer value.
    case Atom::A_Wild:
      if (emitArraySize(ConstArrs, TypeIdx, K))
        break;
      AllArrays = false;
      if (FV != nullptr) {
        FptrInner << "*";
        getQualString(TypeIdx, FptrInner);
      } else {
        if (!EmittedBase) {
          assert(!BaseType.empty());
          EmittedBase = true;
          Ss << BaseType << " ";
        }
        Ss << "*";
        getQualString(TypeIdx, Ss);
      }

      break;
    case Atom::A_Const:
    case Atom::A_Var:
      llvm_unreachable("impossible");
      break;
    }
    TypeIdx++;
  }

  // If the previous variable was an array or
  // if we are leaving an array run, we need to emit the
  // annotation for a stack-array
  if (PrevArr && !ConstArrs.empty())
    addArrayAnnotations(ConstArrs, EndStrs);

  // If the whole type is an array so far, and we haven't emitted
  // a name yet, then emit the name so that it appears before
  // the the stack array type.
  if (PrevArr && !EmittedName && AllArrays) {
    EmittedName = true;
    EndStrs.push_front(" " + UseName);
  }

  if (!EmittedBase) {
    // If we have a FV pointer, then our "base" type is a function pointer type.
    if (FV) {
      if (Ss.str().empty()) {
        if (!EmittedName) {
          FptrInner << UseName;
          EmittedName = true;
        }
        for (std::string Str : EndStrs)
          FptrInner << Str;
        EndStrs.clear();
      }
      bool EmitFVName = !FptrInner.str().empty();
      if (EmitFVName)
        Ss << FV->mkString(CS, true, false, false, false, FptrInner.str());
      else
        Ss << FV->mkString(CS, false);
    } else if (TypedefLevelInfo.HasTypedef) {
      std::ostringstream Buf;
      getQualString(TypedefLevelInfo.TypedefLevel, Buf);
      auto Name = TypedefLevelInfo.TypedefName;
      Ss << Buf.str() << Name;
    } else {
      Ss << BaseType;
    }
  }

  // Add closing elements to type
  for (std::string Str : EndStrs) {
    Ss << Str;
  }

  // No space after itype.
  if (!EmittedName && !UseName.empty())
    Ss << " " << UseName;

  // Final array dropping.
  if (!ConstArrs.empty()) {
    std::deque<std::string> ArrStrs;
    addArrayAnnotations(ConstArrs, ArrStrs);
    for (std::string Str : ArrStrs)
      Ss << Str;
  }

  if (IsReturn && !ForItype)
    Ss << " ";

  return Ss.str();
}

bool PVConstraint::addArgumentConstraint(ConstraintVariable *DstCons,
                                         ProgramInfo &Info) {
  if (this->Parent == nullptr) {
    bool RetVal = false;
    if (isPartOfFunctionPrototype()) {
      RetVal = ArgumentConstraints.insert(DstCons).second;
      if (RetVal && this->HasEqArgumentConstraints) {
        constrainConsVarGeq(DstCons, this, Info.getConstraints(), nullptr,
                            Same_to_Same, true, &Info);
      }
    }
    return RetVal;
  }
  return this->Parent->addArgumentConstraint(DstCons, Info);
}

const CVarSet &PVConstraint::getArgumentConstraints() const {
  return ArgumentConstraints;
}

FunctionVariableConstraint::FunctionVariableConstraint(
    FunctionVariableConstraint *Ot, Constraints &CS)
    : ConstraintVariable(ConstraintVariable::FunctionVariable, Ot->OriginalType,
                         Ot->getName()) {
  this->IsStatic = Ot->IsStatic;
  this->FileName = Ot->FileName;
  this->Hasbody = Ot->Hasbody;
  this->Hasproto = Ot->Hasproto;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  this->IsFunctionPtr = Ot->IsFunctionPtr;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  this->ReturnVar = FVComponentVariable(&Ot->ReturnVar, CS);
  // Make copy of ParameterCVs too.
  for (auto &ParmPv : Ot->ParamVars)
    this->ParamVars.push_back(FVComponentVariable(&ParmPv, CS));
  this->Parent = Ot;
}

// This describes a function, either a function pointer or a function
// declaration itself. Require constraint variables for each argument and
// return, even those that aren't pointer types, since we may need to
// re-emit the function signature as a type.
FunctionVariableConstraint::FunctionVariableConstraint(DeclaratorDecl *D,
                                                       ProgramInfo &I,
                                                       const ASTContext &C)
    : FunctionVariableConstraint(
          D->getType().getTypePtr(), D,
          D->getDeclName().isIdentifier() ? std::string(D->getName()) : "", I,
          C, D->getTypeSourceInfo()) {}

FunctionVariableConstraint::FunctionVariableConstraint(TypedefDecl *D,
                                                       ProgramInfo &I,
                                                       const ASTContext &C)
  : FunctionVariableConstraint(D->getUnderlyingType().getTypePtr(), nullptr,
                               D->getNameAsString(), I, C,
                               D->getTypeSourceInfo()) {}

FunctionVariableConstraint::FunctionVariableConstraint(const Type *Ty,
                                                       DeclaratorDecl *D,
                                                       std::string N,
                                                       ProgramInfo &I,
                                                       const ASTContext &Ctx,
                                                       TypeSourceInfo *TSInfo)
    : ConstraintVariable(ConstraintVariable::FunctionVariable, tyToStr(Ty), N),
      Parent(nullptr) {
  QualType RT, RTIType;
  Hasproto = false;
  Hasbody = false;
  FileName = "";
  HasEqArgumentConstraints = false;
  IsFunctionPtr = true;
  TypeParams = 0;

  // Metadata about function.
  FunctionDecl *FD = nullptr;
  if (D)
    FD = dyn_cast<FunctionDecl>(D);
  if (FD) {
    // FunctionDecl::hasBody will return true if *any* declaration in the
    // declaration chain has a body, which is not what we want to record.
    // We want to record if *this* declaration has a body. To do that,
    // we'll check if the declaration that has the body is different
    // from the current declaration.
    const FunctionDecl *OFd = nullptr;
    if (FD->hasBody(OFd) && OFd == FD)
      Hasbody = true;
    IsStatic = !(FD->isGlobal());
    ASTContext *TmpCtx = const_cast<ASTContext *>(&Ctx);
    auto PSL = PersistentSourceLoc::mkPSL(D, *TmpCtx);
    FileName = PSL.getFileName();
    IsFunctionPtr = false;
  }

  bool ReturnHasItype = false;
  // ConstraintVariables for the parameters.
  if (Ty->isFunctionPointerType()) {
    // Is this a function pointer definition?
    llvm_unreachable("should not hit this case");
  } else if (Ty->isFunctionProtoType()) {
    // Is this a function?
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    assert(FT != nullptr);
    // If we don't have a function declaration, but the return does have an
    // itype, then use the itype as the return type. This is so that we don't
    // drop itype annotation on function pointer return types.
    RT = FT->getReturnType();
    ReturnHasItype = FT->getReturnAnnots().getInteropTypeExpr();
    if (ReturnHasItype)
      RTIType = FT->getReturnAnnots().getInteropTypeExpr()->getType();

    FunctionTypeLoc FTL;
    if (TSInfo != nullptr)
      if (TypeLoc TL = TSInfo->getTypeLoc())
        FTL = getBaseTypeLoc(TL).getAs<FunctionTypeLoc>();

    // Extract the types for the parameters to this function. If the parameter
    // has a bounds expression associated with it, substitute the type of that
    // bounds expression for the other type.
    for (unsigned J = 0; J < FT->getNumParams(); J++) {
      // Same conditional as we had for the return type. If we don't have a
      // function declaration then the itype for the parameter is used as if it
      // were the parameter's primary type.
      QualType QT = FT->getParamType(J);
      QualType ITypeT;
      bool ParamHasItype = FT->getParamAnnots(J).getInteropTypeExpr();
      if (ParamHasItype)
        ITypeT = FT->getParamAnnots(J).getInteropTypeExpr()->getType();

      DeclaratorDecl *ParmVD = nullptr;
      if (FD && J < FD->getNumParams())
        ParmVD = FD->getParamDecl(J);
      if (ParmVD == nullptr && FTL && J < FTL.getNumParams())
        ParmVD = FTL.getParam(J);
      std::string PName = ParmVD ? ParmVD->getName().str() : "";

      auto ParamVar =
          FVComponentVariable(QT, ITypeT, ParmVD, PName, I, Ctx, &N, ParamHasItype);
      int GenericIdx = ParamVar.ExternalConstraint->getGenericIndex();
      if (GenericIdx >= 0)
        TypeParams = std::max(TypeParams, GenericIdx + 1);
      ParamVars.push_back(ParamVar);
    }

    Hasproto = true;
  } else if (Ty->isFunctionNoProtoType()) {
    const FunctionNoProtoType *FT = Ty->getAs<FunctionNoProtoType>();
    assert(FT != nullptr);
    RT = FT->getReturnType();
  } else {
    llvm_unreachable("don't know what to do");
  }

  // ConstraintVariable for the return.
  ReturnVar = FVComponentVariable(RT, RTIType, D, RETVAR, I, Ctx, &N, ReturnHasItype);
  int GenericIdx = ReturnVar.ExternalConstraint->getGenericIndex();
  if (GenericIdx >= 0)
    TypeParams = std::max(TypeParams, GenericIdx + 1);
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS,
                                                 const std::string &Rsn) const {
  constrainToWild(CS, Rsn, nullptr);
}

void FunctionVariableConstraint::constrainToWild(
    Constraints &CS, const std::string &Rsn, PersistentSourceLoc *PL) const {
  ReturnVar.ExternalConstraint->constrainToWild(CS, Rsn, PL);

  for (const auto &V : ParamVars)
    V.ExternalConstraint->constrainToWild(CS, Rsn, PL);
}
bool FunctionVariableConstraint::anyChanges(const EnvironmentMap &E) const {
  return ReturnVar.ExternalConstraint->anyChanges(E) ||
         llvm::any_of(ParamVars, [&E](FVComponentVariable CV) {
           return CV.ExternalConstraint->anyChanges(E);
         });
}

bool
FunctionVariableConstraint::isSolutionChecked(const EnvironmentMap &E) const {
  return ReturnVar.ExternalConstraint->isSolutionChecked(E) ||
         llvm::any_of(ParamVars, [&E](FVComponentVariable CV) {
           return CV.ExternalConstraint->isSolutionChecked(E);
         });
}

bool FunctionVariableConstraint::isSolutionFullyChecked(
  const EnvironmentMap &E) const {
  return ReturnVar.ExternalConstraint->isSolutionChecked(E) &&
         llvm::all_of(ParamVars, [&E](FVComponentVariable CV) {
           return CV.ExternalConstraint->isSolutionChecked(E);
         });
}

bool FunctionVariableConstraint::hasWild(const EnvironmentMap &E,
                                         int AIdx) const {
  return ReturnVar.ExternalConstraint->hasWild(E, AIdx);
}

bool FunctionVariableConstraint::hasArr(const EnvironmentMap &E,
                                        int AIdx) const {
  return ReturnVar.ExternalConstraint->hasArr(E, AIdx);
}

bool FunctionVariableConstraint::hasNtArr(const EnvironmentMap &E,
                                          int AIdx) const {
  return ReturnVar.ExternalConstraint->hasNtArr(E, AIdx);
}

FVConstraint *FunctionVariableConstraint::getCopy(Constraints &CS) {
  return new FVConstraint(this, CS);
}

void PVConstraint::equateArgumentConstraints(ProgramInfo &Info) {
  if (HasEqArgumentConstraints) {
    return;
  }
  HasEqArgumentConstraints = true;
  constrainConsVarGeq(this, this->ArgumentConstraints, Info.getConstraints(),
                      nullptr, Same_to_Same, true, &Info);

  if (this->FV != nullptr) {
    this->FV->equateArgumentConstraints(Info);
  }
}

void FunctionVariableConstraint::equateFVConstraintVars(
    ConstraintVariable *CV, ProgramInfo &Info) const {
  if (FVConstraint *FVCons = dyn_cast<FVConstraint>(CV)) {
    for (auto &PCon : FVCons->ParamVars)
      PCon.InternalConstraint->equateArgumentConstraints(Info);
    FVCons->ReturnVar.InternalConstraint->equateArgumentConstraints(Info);
  }
}

void FunctionVariableConstraint::equateArgumentConstraints(ProgramInfo &Info) {
  if (HasEqArgumentConstraints) {
    return;
  }

  HasEqArgumentConstraints = true;

  // Equate arguments and parameters vars.
  this->equateFVConstraintVars(this, Info);

  // Is this not a function pointer?
  if (!IsFunctionPtr) {
    FVConstraint *DefnCons = nullptr;

    // Get appropriate constraints based on whether the function is static or
    // not.
    if (IsStatic) {
      DefnCons = Info.getStaticFuncConstraint(Name, FileName);
    } else {
      DefnCons = Info.getExtFuncDefnConstraint(Name);
    }
    assert(DefnCons != nullptr);

    // Equate arguments and parameters vars.
    this->equateFVConstraintVars(DefnCons, Info);
  }
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                const std::string &Rsn) const {
  constrainToWild(CS, Rsn, nullptr);
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                const std::string &Rsn,
                                                PersistentSourceLoc *PL) const {
  // Find the first VarAtom. All atoms before this are ConstAtoms, so
  // constraining them isn't useful;
  VarAtom *FirstVA = nullptr;
  for (Atom *A : Vars)
    if (isa<VarAtom>(A)) {
      FirstVA = cast<VarAtom>(A);
      break;
    }

  // Constrains the outer VarAtom to WILD. Inner pointer levels are
  // implicitly WILD because of implication constraints.
  if (FirstVA)
    CS.addConstraint(CS.createGeq(FirstVA, CS.getWild(), Rsn, PL, true));

  if (FV)
    FV->constrainToWild(CS, Rsn, PL);
}

void PointerVariableConstraint::constrainIdxTo(Constraints &CS, ConstAtom *C,
                                               unsigned int Idx, bool DoLB,
                                               bool Soft) {
  assert(C == CS.getPtr() || C == CS.getArr() || C == CS.getNTArr());

  if (Vars.size() > Idx) {
    Atom *A = Vars[Idx];
    if (VarAtom *VA = dyn_cast<VarAtom>(A)) {
      if (DoLB)
        CS.addConstraint(CS.createGeq(VA, C, false, Soft));
      else
        CS.addConstraint(CS.createGeq(C, VA, false, Soft));
    } else if (ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
      if (DoLB) {
        if (*CA < *C) {
          llvm::errs() << "Warning: " << CA->getStr() << " not less than "
                       << C->getStr() << "\n";
          //assert(CA == CS.getWild()); // Definitely bogus if not.
        }
      } else if (*C < *CA) {
        llvm::errs() << "Warning: " << C->getStr() << " not less than "
                     << CA->getStr() << "\n";
        //assert(CA == CS.getWild()); // Definitely bogus if not.
      }
    }
  }
}

void PointerVariableConstraint::constrainOuterTo(Constraints &CS, ConstAtom *C,
                                                 bool DoLB, bool Soft) {
  constrainIdxTo(CS,C,0,DoLB, Soft);
}

bool PointerVariableConstraint::anyArgumentIsWild(const EnvironmentMap &E) {
  return llvm::any_of(ArgumentConstraints, [&E](ConstraintVariable *CV) {
    return !CV->isSolutionChecked(E);
  });
}

bool
PointerVariableConstraint::isSolutionChecked(const EnvironmentMap &E) const {
  return (FV && FV->isSolutionChecked(E)) ||
         llvm::any_of(Vars, [this, &E](Atom *A){
           return !isa<WildAtom>(getSolution(A, E));
         });
}

bool PointerVariableConstraint::isSolutionFullyChecked(
  const EnvironmentMap &E) const {
  return (!FV || FV->isSolutionChecked(E)) &&
         llvm::all_of(Vars, [this, &E](Atom *A){
           return !isa<WildAtom>(getSolution(A, E));
         });
}

bool PointerVariableConstraint::anyChanges(const EnvironmentMap &E) const {
  // If it was not checked in the input, then it has changed if it now has a
  // checked type.
  bool PtrChanged = false;

  // Are there any non-WILD pointers?
  for (unsigned I = 0; I < Vars.size(); I++) {
    ConstAtom *SrcType = SrcVars[I];
    const ConstAtom *SolutionType = getSolution(Vars[I], E);
    PtrChanged |= SrcType != SolutionType;
  }

  if (FV)
    PtrChanged |= FV->anyChanges(E);

  return PtrChanged;
}

PVConstraint *PointerVariableConstraint::getCopy(Constraints &CS) {
  return new PointerVariableConstraint(this, CS);
}

const ConstAtom *
PointerVariableConstraint::getSolution(const Atom *A,
                                       const EnvironmentMap &E) const {
  const ConstAtom *CS = nullptr;
  if (const ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
    CS = CA;
  } else if (const VarAtom *VA = dyn_cast<VarAtom>(A)) {
    // If this is a VarAtom?, we need to fetch from solution i.e., environment.
    CS = E.at(const_cast<VarAtom *>(VA)).first;
  }
  assert(CS != nullptr && "Atom should be either const or var");
  return CS;
}

bool PointerVariableConstraint::hasWild(const EnvironmentMap &E,
                                        int AIdx) const {
  int VarIdx = 0;
  for (const auto &C : Vars) {
    const ConstAtom *CS = getSolution(C, E);
    if (isa<WildAtom>(CS))
      return true;
    if (VarIdx == AIdx)
      break;
    VarIdx++;
  }

  if (FV)
    return FV->hasWild(E, AIdx);

  return false;
}

bool PointerVariableConstraint::hasArr(const EnvironmentMap &E,
                                       int AIdx) const {
  int VarIdx = 0;
  for (const auto &C : Vars) {
    const ConstAtom *CS = getSolution(C, E);
    if (isa<ArrAtom>(CS))
      return true;
    if (VarIdx == AIdx)
      break;
    VarIdx++;
  }

  if (FV)
    return FV->hasArr(E, AIdx);

  return false;
}

bool PointerVariableConstraint::hasNtArr(const EnvironmentMap &E,
                                         int AIdx) const {
  int VarIdx = 0;
  for (const auto &C : Vars) {
    const ConstAtom *CS = getSolution(C, E);
    if (isa<NTArrAtom>(CS))
      return true;
    if (VarIdx == AIdx)
      break;
    VarIdx++;
  }

  if (FV)
    return FV->hasNtArr(E, AIdx);

  return false;
}

bool PointerVariableConstraint::getArrPresent() const {
  return llvm::any_of(ArrSizes,
                      [](auto E) { return E.second.first != O_Pointer; });
}

bool PointerVariableConstraint::isTopCvarUnsizedArr() const {
  if (ArrSizes.find(0) != ArrSizes.end()) {
    return ArrSizes.at(0).first != O_SizedArray;
  }
  return true;
}

bool PointerVariableConstraint::hasSomeSizedArr() const {
  for (auto &AS : ArrSizes) {
    if (AS.second.first == O_SizedArray || AS.second.second == O_UnSizedArray) {
      return true;
    }
  }
  return false;
}

bool PointerVariableConstraint::solutionEqualTo(Constraints &CS,
                                                const ConstraintVariable *CV,
                                                bool ComparePtyp) const {
  bool Ret = false;
  if (CV != nullptr) {
    if (const auto *PV = dyn_cast<PVConstraint>(CV)) {
      auto &OthCVars = PV->Vars;
      if (getIsGeneric() || PV->getIsGeneric() ||
          Vars.size() == OthCVars.size()) {
        Ret = true;

        auto I = Vars.begin();
        auto J = OthCVars.begin();
        // Special handling for zero width arrays so they can compare equal to
        // ARR or PTR.
        if (ComparePtyp && IsZeroWidthArray) {
          assert(I != Vars.end() && "Zero width array cannot be base type.");
          assert("Zero width arrays should be encoded as PTR." &&
                 CS.getAssignment(*I) == CS.getPtr());
          ConstAtom *JAtom = CS.getAssignment(*J);
          // Zero width array can compare as either ARR or PTR
          if (JAtom != CS.getArr() && JAtom != CS.getPtr())
            Ret = false;
          ++I;
          ++J;
        }
        // Compare Vars to see if they are same.
        while (Ret && I != Vars.end() && J != OthCVars.end()) {
          ConstAtom *IAssign = CS.getAssignment(*I);
          ConstAtom *JAssign = CS.getAssignment(*J);
          if (ComparePtyp) {
            if (IAssign != JAssign) {
              Ret = false;
              break;
            }
          } else {
            bool IIsWild = isa<WildAtom>(IAssign);
            bool JIsWild = isa<WildAtom>(JAssign);
            if (IIsWild != JIsWild) {
              Ret = false;
              break;
            }
          }
          ++I;
          ++J;
        }

        if (Ret) {
          FVConstraint *OtherFV = PV->getFV();
          if (FV != nullptr && OtherFV != nullptr) {
            Ret = FV->solutionEqualTo(CS, OtherFV, ComparePtyp);
          } else if (FV != nullptr || OtherFV != nullptr) {
            // One of them has FV null.
            Ret = false;
          }
        }
      }
    } else if (FV && Vars.size() == 1) {
      // If this a function pointer and we're comparing it to a FVConstraint,
      // then solutions can still be equal.
      Ret = FV->solutionEqualTo(CS, CV, ComparePtyp);
    }
  }
  return Ret;
}

void FunctionVariableConstraint::print(raw_ostream &O) const {
  O << "( ";
  ReturnVar.InternalConstraint->print(O);
  O << ", ";
  ReturnVar.ExternalConstraint->print(O);
  O << " )";
  O << " " << Name << " ";
  for (const auto &I : ParamVars) {
    O << "( ";
    I.InternalConstraint->print(O);
    O << ", ";
    I.ExternalConstraint->print(O);
    O << " )";
  }
}

void FunctionVariableConstraint::dumpJson(raw_ostream &O) const {
  O << "{\"FunctionVar\":{\"ReturnVar\":[";
  bool AddComma = false;
  ReturnVar.InternalConstraint->dumpJson(O);
  O << ", ";
  ReturnVar.ExternalConstraint->dumpJson(O);
  O << "], \"name\":\"" << Name << "\", ";
  O << "\"Parameters\":[";
  AddComma = false;
  for (const auto &I : ParamVars) {
    if (AddComma) {
      O << ",\n";
    }
    O << "[";
    I.InternalConstraint->dumpJson(O);
    O << ", ";
    I.ExternalConstraint->dumpJson(O);
    O << "]";
    AddComma = true;
  }
  O << "]";
  O << "}}";
}

bool FunctionVariableConstraint::srcHasItype() const {
  return ReturnVar.ExternalConstraint->srcHasItype();
}

bool FunctionVariableConstraint::srcHasBounds() const {
  return ReturnVar.ExternalConstraint->srcHasBounds();
}

bool FunctionVariableConstraint::solutionEqualTo(Constraints &CS,
                                                 const ConstraintVariable *CV,
                                                 bool ComparePtyp) const {
  bool Ret = false;
  if (CV != nullptr) {
    if (const auto *OtherFV = dyn_cast<FVConstraint>(CV)) {
      Ret = (numParams() == OtherFV->numParams()) &&
        ReturnVar.solutionEqualTo(CS, OtherFV->getCombineReturn(), ComparePtyp);
      for (unsigned I = 0; I < numParams(); I++) {
        Ret &= getCombineParam(I)->solutionEqualTo(CS,
                                                   OtherFV->getCombineParam(I),
                                                   ComparePtyp);
      }
    } else if (const auto *OtherPV = dyn_cast<PVConstraint>(CV)) {
      // When comparing to a pointer variable, it might be that the pointer is a
      // function pointer. This is handled by PVConstraint::solutionEqualTo.
      return OtherPV->solutionEqualTo(CS, this, ComparePtyp);
    }
  }
  return Ret;
}

std::string FunctionVariableConstraint::mkString(Constraints &CS,
                                                 bool EmitName, bool ForItype,
                                                 bool EmitPointee,
                                                 bool UnmaskTypedef,
                                                 std::string UseName) const {
  if (UseName.empty())
    UseName = Name;
  std::string Ret = ReturnVar.mkTypeStr(CS, false);
  std::string Itype = ReturnVar.mkItypeStr(CS);
  if (EmitName) {
    if (UnmaskTypedef)
      // This is done to rewrite the typedef of a function proto
      Ret += UseName;
    else
      Ret += "(" + UseName + ")";
  }
  Ret = Ret + "(";
  std::vector<std::string> ParmStrs;
  for (const auto &I : this->ParamVars)
    ParmStrs.push_back(I.mkString(CS));

  if (ParmStrs.size() > 0) {
    std::ostringstream Ss;

    std::copy(ParmStrs.begin(), ParmStrs.end() - 1,
              std::ostream_iterator<std::string>(Ss, ", "));
    Ss << ParmStrs.back();

    Ret = Ret + Ss.str() + ")";
  } else
    Ret = Ret + "void)";
  return Ret + Itype;
}

// Reverses the direction of CA for function subtyping.
//   TODO: Function pointers forced to be equal right now.
//static ConsAction neg(ConsAction CA) {
//  switch (CA) {
//  case Safe_to_Wild: return Wild_to_Safe;
//  case Wild_to_Safe: return Safe_to_Wild;
//  case Same_to_Same: return Same_to_Same;
//  }
//  // Silencing the compiler.
//  assert(false && "Can never reach here.");
//  return Same_to_Same;
//}

// CA |- R <: L
// Action depends on the kind of constraint (checked, ptyp),
//   which is inferred from the atom type
static void createAtomGeq(Constraints &CS, Atom *L, Atom *R, std::string &Rsn,
                          PersistentSourceLoc *PSL, ConsAction CAct,
                          bool DoEqType) {
  ConstAtom *CAL, *CAR;
  VarAtom *VAL, *VAR;
  ConstAtom *Wild = CS.getWild();

  CAL = clang::dyn_cast<ConstAtom>(L);
  CAR = clang::dyn_cast<ConstAtom>(R);
  VAL = clang::dyn_cast<VarAtom>(L);
  VAR = clang::dyn_cast<VarAtom>(R);

  if (CAR != nullptr && CAL != nullptr) {
    // These checks were used to verify that the relationships between constant
    // atom were valid. While we do not generally intend to rewrite code in a
    // way that violates these relationships, it is possible for a user of 3C
    // to manually modify their code so that it does while still having valid
    // CheckedC code.
    //if (DoEqType) { // Check equality, no matter the atom.
    //  assert(*CAR == *CAL && "Invalid: RHS ConstAtom != LHS ConstAtom");
    //} else {
    //  if (CAL != Wild && CAR != Wild) { // pType atom, disregard CAct.
    //    assert(!(*CAL < *CAR) && "Invalid: LHS ConstAtom < RHS ConstAtom");
    //  } else { // Checked atom (Wild/Ptr); respect CAct.
    //    switch (CAct) {
    //    case Same_to_Same:
    //      assert(*CAR == *CAL && "Invalid: RHS ConstAtom != LHS ConstAtom");
    //      break;
    //    case Safe_to_Wild:
    //      assert(!(*CAL < *CAR) && "LHS ConstAtom < RHS ConstAtom");
    //      break;
    //    case Wild_to_Safe:
    //      assert(!(*CAR < *CAL) && "RHS ConstAtom < LHS ConstAtom");
    //      break;
    //    }
    //  }
    //}
  } else if (VAL != nullptr && VAR != nullptr) {
    switch (CAct) {
    case Same_to_Same:
      // Equality for checked.
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
      // Not for ptyp.
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      // Unless indicated.
      if (DoEqType)
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      break;
    case Safe_to_Wild:
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      if (DoEqType) {
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      }
      break;
    case Wild_to_Safe:
      if (!DisableRDs) {
        // Note: reversal.
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
      } else {
        // Add edges both ways.
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
      }
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      if (DoEqType) {
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      }
      break;
    }
  } else {
    // This should be a checked/unchecked constraint.
    if (CAL == Wild || CAR == Wild) {
      switch (CAct) {
      case Same_to_Same:
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        break;
      case Safe_to_Wild:
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        if (DoEqType)
          CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        break;
      case Wild_to_Safe:
        if (!DisableRDs) {
          // Note: reversal.
          CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        } else {
          CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        }
        if (DoEqType)
          CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        break;
      }
    } else {
      // This should be a pointer-type constraint.
      switch (CAct) {
      case Same_to_Same:
      case Safe_to_Wild:
      case Wild_to_Safe:
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
        if (DoEqType)
          CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
        break;
      }
    }
  }
}

// Generate constraints according to CA |- RHS <: LHS.
// If doEqType is true, then also do CA |- LHS <: RHS.
void constrainConsVarGeq(ConstraintVariable *LHS, ConstraintVariable *RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool DoEqType, ProgramInfo *Info,
                         bool HandleBoundsKey) {

  // If one of the constraint is NULL, make the other constraint WILD.
  // This can happen when a non-function pointer gets assigned to
  // a function pointer.
  if (LHS == nullptr || RHS == nullptr) {
    std::string Rsn = "Assignment a non-pointer to a pointer";
    if (LHS != nullptr) {
      LHS->constrainToWild(CS, Rsn, PL);
    }
    if (RHS != nullptr) {
      RHS->constrainToWild(CS, Rsn, PL);
    }
    return;
  }

  if (RHS->getKind() == LHS->getKind()) {
    if (FVConstraint *FCLHS = dyn_cast<FVConstraint>(LHS)) {
      if (FVConstraint *FCRHS = dyn_cast<FVConstraint>(RHS)) {

        // This is an assignment between function pointer and
        // function pointer or a function.
        // Force past/future callers of function to use equality constraints.
        FCLHS->equateArgumentConstraints(*Info);
        FCRHS->equateArgumentConstraints(*Info);

        // Constrain the return values covariantly.
        // FIXME: Make neg(CA) here? Function pointers equated.
        constrainConsVarGeq(FCLHS->getExternalReturn(),
                            FCRHS->getExternalReturn(), CS, PL, Same_to_Same,
                            DoEqType, Info, HandleBoundsKey);
        constrainConsVarGeq(FCLHS->getInternalReturn(),
                            FCRHS->getInternalReturn(), CS, PL, Same_to_Same,
                            DoEqType, Info, HandleBoundsKey);

        // Constrain the parameters contravariantly.
        if (FCLHS->numParams() == FCRHS->numParams()) {
          for (unsigned I = 0; I < FCLHS->numParams(); I++) {
            ConstraintVariable *LHSV = FCLHS->getExternalParam(I);
            ConstraintVariable *RHSV = FCRHS->getExternalParam(I);
            // FIXME: Make neg(CA) here? Now: Function pointers equated.
            constrainConsVarGeq(RHSV, LHSV, CS, PL, Same_to_Same, DoEqType,
                                Info, HandleBoundsKey);

            ConstraintVariable *LParam = FCLHS->getInternalParam(I);
            ConstraintVariable *RParam = FCRHS->getInternalParam(I);
            constrainConsVarGeq(RParam, LParam, CS, PL, Same_to_Same, DoEqType,
                                Info, HandleBoundsKey);
          }
        } else {
          // Constrain both to be top.
          std::string Rsn =
              "Assigning from:" + FCRHS->getName() + " to " + FCLHS->getName();
          RHS->constrainToWild(CS, Rsn, PL);
          LHS->constrainToWild(CS, Rsn, PL);
        }
      } else {
        llvm_unreachable("impossible");
      }
    } else if (PVConstraint *PCLHS = dyn_cast<PVConstraint>(LHS)) {
      if (PVConstraint *PCRHS = dyn_cast<PVConstraint>(RHS)) {

        // Add assignment to bounds info graph.
        if (HandleBoundsKey && PCLHS->hasBoundsKey() && PCRHS->hasBoundsKey()) {
          Info->getABoundsInfo().addAssignment(PCLHS->getBoundsKey(),
                                               PCRHS->getBoundsKey());
        }

        std::string Rsn = "";
        // This is to handle function subtyping. Try to add LHS and RHS
        // to each others argument constraints.
        PCLHS->addArgumentConstraint(PCRHS, *Info);
        PCRHS->addArgumentConstraint(PCLHS, *Info);
        // Element-wise constrain PCLHS and PCRHS to be equal.
        CAtoms CLHS = PCLHS->getCvars();
        CAtoms CRHS = PCRHS->getCvars();

        // Only generate constraint if LHS is not a base type.
        if (CLHS.size() != 0) {
          if (CLHS.size() == CRHS.size() || PCLHS->getIsGeneric() ||
              PCRHS->getIsGeneric()) {
            unsigned Max = std::max(CLHS.size(), CRHS.size());
            for (unsigned N = 0; N < Max; N++) {
              Atom *IAtom = PCLHS->getAtom(N, CS);
              Atom *JAtom = PCRHS->getAtom(N, CS);
              if (IAtom == nullptr || JAtom == nullptr)
                break;

              // Get outermost pointer first, using current ConsAction.
              if (N == 0)
                createAtomGeq(CS, IAtom, JAtom, Rsn, PL, CA, DoEqType);
              else {
                // Now constrain the inner ones as equal.
                createAtomGeq(CS, IAtom, JAtom, Rsn, PL, CA, true);
              }
            }
            // Unequal sizes means casting from (say) T** to T*; not safe.
            // unless assigning to a generic type.
          } else {
            // Constrain both to be top.
            std::string Rsn = "Assigning from:" + std::to_string(CRHS.size()) +
                              " depth pointer to " +
                              std::to_string(CLHS.size()) + " depth pointer.";
            PCLHS->constrainToWild(CS, Rsn, PL);
            PCRHS->constrainToWild(CS, Rsn, PL);
          }
          // Equate the corresponding FunctionConstraint.
          constrainConsVarGeq(PCLHS->getFV(), PCRHS->getFV(), CS, PL, CA,
                              DoEqType, Info, HandleBoundsKey);
        }
      } else
        llvm_unreachable("impossible");
    } else
      llvm_unreachable("unknown kind");
  } else {
    // Assigning from a function variable to a pointer variable?
    PVConstraint *PCLHS = dyn_cast<PVConstraint>(LHS);
    FVConstraint *FCRHS = dyn_cast<FVConstraint>(RHS);
    if (PCLHS && FCRHS) {
      if (FVConstraint *FCLHS = PCLHS->getFV()) {
        constrainConsVarGeq(FCLHS, FCRHS, CS, PL, CA, DoEqType, Info,
                            HandleBoundsKey);
      } else {
        std::string Rsn = "Function:" + FCRHS->getName() +
                          " assigned to non-function pointer.";
        LHS->constrainToWild(CS, Rsn, PL);
        RHS->constrainToWild(CS, Rsn, PL);
      }
    } else {
      // Constrain everything in both to wild.
      std::string Rsn = "Assignment to functions from variables";
      LHS->constrainToWild(CS, Rsn, PL);
      RHS->constrainToWild(CS, Rsn, PL);
    }
  }
}

void constrainConsVarGeq(ConstraintVariable *LHS, const CVarSet &RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool DoEqType, ProgramInfo *Info,
                         bool HandleBoundsKey) {
  for (const auto &J : RHS)
    constrainConsVarGeq(LHS, J, CS, PL, CA, DoEqType, Info, HandleBoundsKey);
}

// Given an RHS and a LHS, constrain them to be equal.
void constrainConsVarGeq(const CVarSet &LHS, const CVarSet &RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool DoEqType, ProgramInfo *Info,
                         bool HandleBoundsKey) {
  for (const auto &I : LHS)
    constrainConsVarGeq(I, RHS, CS, PL, CA, DoEqType, Info, HandleBoundsKey);
}

// True if [C] is a PVConstraint that contains at least one Atom (i.e.,
//   it represents a C pointer).
bool isAValidPVConstraint(const ConstraintVariable *C) {
  if (const auto *PV = dyn_cast_or_null<PVConstraint>(C))
    return !PV->getCvars().empty();
  return false;
}

void PointerVariableConstraint::mergeDeclaration(ConstraintVariable *FromCV,
                                                 ProgramInfo &Info,
                                                 std::string &ReasonFailed) {
  PVConstraint *From = dyn_cast<PVConstraint>(FromCV);
  std::vector<Atom *> NewVAtoms;
  std::vector<ConstAtom *> NewSrcAtoms;
  CAtoms CFrom = From->getCvars();
  if (CFrom.size() != Vars.size()) {
    ReasonFailed = "transplanting between pointers with different depths";
    return;
  }
  for (unsigned AtomIdx = 0; AtomIdx < Vars.size(); AtomIdx++) {
    // Take the ConstAtom if merging from a constraint variable with ConstAtoms
    // into a variable with VarAtoms. This case shows up less often with the
    // changes made to allow updating itype pointer types, but it can still
    // happen whenever a pointer other than a function parameter is redeclared
    // with a checked type after an unchecked declaration. For example, an
    // extern global can be redeclared with an itype.
    if (!isa<ConstAtom>(Vars[AtomIdx]) && isa<ConstAtom>(From->Vars[AtomIdx]))
      NewVAtoms.push_back(From->Vars[AtomIdx]);
    else
      NewVAtoms.push_back(Vars[AtomIdx]);

    // If the current variable was wild in the source, and we're merging
    // something that had a checked type, then take the checked type. This is
    // particularly important for itypes since function parameters can be
    // redeclared with an itype.
    if (isa<WildAtom>(SrcVars[AtomIdx]))
      NewSrcAtoms.push_back(From->SrcVars[AtomIdx]);
    else
      NewSrcAtoms.push_back(SrcVars[AtomIdx]);
  }
  assert(Vars.size() == NewVAtoms.size() &&
         SrcVars.size() == NewSrcAtoms.size() &&
         "Merging error, pointer depth change");
  Vars = NewVAtoms;
  SrcVars = NewSrcAtoms;
  if (Name.empty())
    Name = From->Name;
  SrcHasItype = SrcHasItype || From->SrcHasItype;
  if (!From->ItypeStr.empty())
    ItypeStr = From->ItypeStr;

  // Merge Bounds Related information.
  if (hasBoundsKey() && From->hasBoundsKey())
    Info.getABoundsInfo().mergeBoundsKey(getBoundsKey(), From->getBoundsKey());
  if (!From->BoundsAnnotationStr.empty())
    BoundsAnnotationStr = From->BoundsAnnotationStr;

  if (From->GenericIndex >= 0)
    GenericIndex = From->GenericIndex;
  if (FV) {
    assert(From->FV);
    FV->mergeDeclaration(From->FV, Info, ReasonFailed);
    if (ReasonFailed != "") {
      ReasonFailed += " within the referenced function";
      return;
    }
  }
}

Atom *PointerVariableConstraint::getAtom(unsigned AtomIdx, Constraints &CS) {
  if (AtomIdx < Vars.size()) {
    // If index is in bounds, just return the atom.
    return Vars[AtomIdx];
  }
  if (getIsGeneric() && AtomIdx == Vars.size()) {
    // Polymorphic types don't know how "deep" their pointers are beforehand so,
    // we need to create new atoms for new pointer levels on the fly.
    std::string Stars(Vars.size(), '*');
    Atom *A = CS.getFreshVar(Name + Stars, VarAtom::V_Other);
    Vars.push_back(A);
    SrcVars.push_back(CS.getWild());
    return A;
  }
  return nullptr;
}

void
PointerVariableConstraint::equateWithItype(ProgramInfo &I, bool ForceEquate) {
  Constraints &CS = I.getConstraints();
  assert(SrcVars.size() == Vars.size());
  for (unsigned VarIdx = 0; VarIdx < Vars.size(); VarIdx++) {
    ConstAtom *CA = SrcVars[VarIdx];
    if (isa<WildAtom>(CA))
      CS.addConstraint(CS.createGeq(Vars[VarIdx], CA, true));
    else
      Vars[VarIdx] = SrcVars[VarIdx];
  }
  if (FV) {
    FV->equateWithItype(I, ForceEquate);
  }
}

void FunctionVariableConstraint::mergeDeclaration(ConstraintVariable *FromCV,
                                                  ProgramInfo &I,
                                                  std::string &ReasonFailed) {
  // `this`: is the declaration the tool saw first.
  // `FromCV`: is the declaration seen second

  FVConstraint *From = dyn_cast<FVConstraint>(FromCV);
  assert(From != nullptr);
  assert("this should have more params" &&
         this->numParams() >= From->numParams());

  // transferable basic info
  Hasbody |= From->Hasbody;
  if (Name.empty()) Name = From->Name;

  // Merge returns.
  ReturnVar.mergeDeclaration(&From->ReturnVar, I, ReasonFailed);
  if (ReasonFailed != "") {
    ReasonFailed += " for return value";
    return;
  }

  // Merge params.
  for (unsigned J = 0; J < From->numParams(); J++) {
    ParamVars[J].mergeDeclaration(&From->ParamVars[J], I, ReasonFailed);
    if (ReasonFailed != "") {
      ReasonFailed += " for parameter " + std::to_string(J);
      return;
    }
  }
}

bool FunctionVariableConstraint::isOriginallyChecked() const {
  return ReturnVar.ExternalConstraint->isOriginallyChecked();
}

void
FunctionVariableConstraint::equateWithItype(ProgramInfo &I, bool ForceEquate) {
  ReturnVar.equateWithItype(I, ForceEquate);
  for (auto Param : ParamVars)
    Param.equateWithItype(I, ForceEquate);
}

void FVComponentVariable::mergeDeclaration(FVComponentVariable *From,
                                           ProgramInfo &I,
                                           std::string &ReasonFailed) {
  if (InternalConstraint == ExternalConstraint
      && From->InternalConstraint != From->ExternalConstraint) {
    // Special handling for merging declarations where the original declaration
    // was allocated using the same constraint variable for internal and
    // external constraints but a subsequent declaration allocated separate
    // variables. This can happen if the second declaration declares an itype
    // but the original declaration does not.
    From->InternalConstraint->mergeDeclaration(InternalConstraint, I,
                                               ReasonFailed);
    InternalConstraint = From->InternalConstraint;
  } else {
    InternalConstraint->mergeDeclaration(From->InternalConstraint, I,
                                         ReasonFailed);
  }
  if (ReasonFailed != ""){
    ReasonFailed += " during internal merge";
    return;
  }
  ExternalConstraint->mergeDeclaration(From->ExternalConstraint, I,
                                       ReasonFailed);
}

std::string
FVComponentVariable::mkString(Constraints &CS, bool EmitName) const {
  return mkTypeStr(CS, EmitName) + mkItypeStr(CS);
}

std::string
FVComponentVariable::mkTypeStr(Constraints &CS, bool EmitName,
                               std::string UseName) const {
  // if checked or given new name, generate type
  if (hasCheckedSolution(CS) || (EmitName && !UseName.empty()))
    return ExternalConstraint->mkString(CS, EmitName, false,
                                        false, false, UseName);
  // if no need to generate type, try to use source
  if (!SourceDeclaration.empty())
    return SourceDeclaration;
  // if no source and no name, generate nameless type
  if (EmitName && ExternalConstraint->getName().empty())
    return ExternalConstraint->getOriginalTy();
  // if no source and a have a needed name, generate named type
  if (EmitName)
    return ExternalConstraint->getRewritableOriginalTy()
           + ExternalConstraint->getName();
  // if no source and don't need a name, generate type ready for one
  return ExternalConstraint->getRewritableOriginalTy();
}

std::string FVComponentVariable::mkItypeStr(Constraints &CS) const {
  if (hasItypeSolution(CS))
    return " : itype(" + ExternalConstraint->mkString(CS, false, true) + ")";
  return "";
}

bool FVComponentVariable::hasCheckedSolution(Constraints &CS) const {
  // If the external constraint variable is checked, then the variable should
  // be advertised as checked to callers. If the internal and external
  // constraint variables solve to the same type, then they are both checked and
  // we can use a _Ptr type.
  return ExternalConstraint->isSolutionChecked(CS.getVariables()) &&
         ((ExternalConstraint->srcHasItype() &&
           InternalConstraint->isSolutionChecked(CS.getVariables())) ||
          ExternalConstraint->anyChanges(CS.getVariables())) &&
         InternalConstraint->solutionEqualTo(CS, ExternalConstraint);
}

bool FVComponentVariable::hasItypeSolution(Constraints &CS) const {
  // As in hasCheckedSolution, we want the variable to be advertised as checked
  // if the external variable is checked. If the external is checked, but the
  // internal is not equal to the external, then the internal is unchecked, so
  // have to use an itype instead of a _Ptr type.
  return ExternalConstraint->isSolutionChecked(CS.getVariables()) &&
         ExternalConstraint->anyChanges(CS.getVariables()) &&
         !InternalConstraint->solutionEqualTo(CS, ExternalConstraint);
}

FVComponentVariable::FVComponentVariable(const QualType &QT,
                                         const QualType &ITypeT,
                                         clang::DeclaratorDecl *D,
                                         std::string N, ProgramInfo &I,
                                         const ASTContext &C,
                                         std::string *InFunc, bool HasItype) {
  ExternalConstraint = new PVConstraint(QT, D, N, I, C, InFunc, -1, HasItype,
                                        nullptr, ITypeT);
  InternalConstraint = new PVConstraint(QT, D, N, I, C, InFunc, -1, HasItype,
                                        nullptr, ITypeT);
  bool EquateChecked = (QT->isVoidPointerType() || QT->isFunctionPointerType());
  linkInternalExternal(I, EquateChecked);

  // Save the original source for the declaration if this is a param
  // declaration. This lets us avoid macro expansion in function pointer
  // parameters similarly to how we do it for pointers in regular function
  // declarations.
  if (D && D->getType() == QT) {
    SourceRange SR = D->getSourceRange();
    SourceDeclaration = SR.isValid() ? getSourceText(SR, C) : "";
  }
}

void
FVComponentVariable::equateWithItype(ProgramInfo &I, bool ForceEquate) const {
  Constraints &CS = I.getConstraints();
  bool IsGeneric = ExternalConstraint->getIsGeneric();
  bool HasBounds = ExternalConstraint->srcHasBounds();
  bool HasItype = ExternalConstraint->srcHasItype();
  if (HasItype && (ForceEquate || IsGeneric || HasBounds)) {
    ExternalConstraint->equateWithItype(I, ForceEquate);
    if (ExternalConstraint != InternalConstraint)
      linkInternalExternal(I, false);
    if (IsGeneric)
      InternalConstraint->constrainToWild(CS, "Internal constraint for generic function.");
  }
}

void FVComponentVariable::linkInternalExternal(ProgramInfo &I,
                                               bool EquateChecked) const {
  Constraints &CS = I.getConstraints();
  for (unsigned J = 0; J < InternalConstraint->getCvars().size(); J++) {
    Atom *InternalA = InternalConstraint->getCvars()[J];
    Atom *ExternalA = ExternalConstraint->getCvars()[J];
    if (isa<VarAtom>(InternalA) || isa<VarAtom>(ExternalA)) {
      // Equate pointer types for internal and external parameter constraint
      // variables.
      CS.addConstraint(CS.createGeq(InternalA, ExternalA, false));
      CS.addConstraint(CS.createGeq(ExternalA, InternalA, false));

      if (!isa<ConstAtom>(ExternalA)) {
        // Constrain Internal >= External. If external solves to wild, then so
        // does the internal. Not that this doesn't mean any unsafe external
        // use causes the internal variable to be wild because the external
        // variable solves to WILD only when there is an unsafe use that
        // cannot be resolved by inserting casts.
        CS.addConstraint(CS.createGeq(InternalA, ExternalA, true));

        // Atoms of return constraint variables are unified after the first
        // level. This is because CheckedC does not allow assignment from e.g.
        // a function return of type `int ** : itype(_Ptr<_Ptr<int>>)` to a
        // variable with type `int **`.
        if (DisableFunctionEdges || DisableRDs || EquateChecked || 
            (ExternalConstraint->getName() == RETVAR && J > 0))
          CS.addConstraint(CS.createGeq(ExternalA, InternalA, true));
      }
    }
  }
  if (FVConstraint *ExtFV = ExternalConstraint->getFV()) {
    FVConstraint *IntFV = InternalConstraint->getFV();
    assert(IntFV != nullptr);
    constrainConsVarGeq(ExtFV, IntFV, CS, nullptr, Same_to_Same, true,&I);
  }
}

bool FVComponentVariable::solutionEqualTo(Constraints &CS,
                                          const FVComponentVariable *Other,
                                          bool ComparePtyp) const {
  bool InternalEq = getInternal()->solutionEqualTo(CS, Other->getInternal(),
                                                   ComparePtyp);
  bool ExternalEq = getExternal()->solutionEqualTo(CS, Other->getExternal(),
                                                   ComparePtyp);
  return InternalEq || ExternalEq;
}

FVComponentVariable::FVComponentVariable(FVComponentVariable *Ot,
                                         Constraints &CS) {
  InternalConstraint = Ot->InternalConstraint->getCopy(CS);
  ExternalConstraint = Ot->ExternalConstraint->getCopy(CS);
}
