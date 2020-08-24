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

#include "llvm/ADT/StringSwitch.h"
#include "clang/Lex/Lexer.h"
#include <sstream>

#include "clang/CConv/ConstraintVariables.h"
#include "clang/CConv/ProgramInfo.h"
#include "clang/CConv/CCGlobalOptions.h"

using namespace clang;

std::string ConstraintVariable::getRewritableOriginalTy() {
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
bool ConstraintVariable::isChecked(EnvironmentMap &E) {
  return getIsOriginallyChecked() || anyChanges(E);
}

PointerVariableConstraint *
PointerVariableConstraint::getWildPVConstraint(Constraints &CS) {
  static PointerVariableConstraint *GlobalWildPV = nullptr;
  if (GlobalWildPV == nullptr) {
    CAtoms NewVA;
    NewVA.push_back(CS.getWild());
    GlobalWildPV =
        new PVConstraint(NewVA, "unsigned", "wildvar", nullptr,
                         false, false, "");
  }
  return GlobalWildPV;
}

PointerVariableConstraint *
PointerVariableConstraint::getPtrPVConstraint(Constraints &CS) {
  static PointerVariableConstraint *GlobalPtrPV = nullptr;
  if (GlobalPtrPV == nullptr) {
    CAtoms NewVA;
    NewVA.push_back(CS.getPtr());
    GlobalPtrPV =
        new PVConstraint(NewVA, "unsigned", "ptrvar", nullptr,
                         false, false, "");
  }
  return GlobalPtrPV;
}

PointerVariableConstraint *
PointerVariableConstraint::getNonPtrPVConstraint(Constraints &CS) {
  static PointerVariableConstraint *GlobalNonPtrPV = nullptr;
  if (GlobalNonPtrPV == nullptr) {
    CAtoms NewVA; // empty -- represents a base type
    GlobalNonPtrPV =
        new PVConstraint(NewVA, "unsigned", "basevar", nullptr,
                         false, false, "");
  }
  return GlobalNonPtrPV;
}

PointerVariableConstraint *
PointerVariableConstraint::getNamedNonPtrPVConstraint(StringRef name,
                                                      Constraints &CS) {
  CAtoms NewVA; // empty -- represents a base type
  return new PVConstraint(NewVA, "unsigned", name, nullptr,
                          false, false, "");
}

PointerVariableConstraint::
    PointerVariableConstraint(PointerVariableConstraint *Ot,
                              Constraints &CS) :
    ConstraintVariable(ConstraintVariable::PointerVariable,
                       Ot->BaseType, Ot->Name),
    FV(nullptr), partOFFuncPrototype(Ot->partOFFuncPrototype) {
  this->arrSizes = Ot->arrSizes;
  this->ArrPresent = Ot->ArrPresent;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  this->ValidBoundsKey = Ot->ValidBoundsKey;
  this->BKey = Ot->BKey;
  // Make copy of the vars only for VarAtoms.
  for (auto *CV : Ot->vars) {
    if (ConstAtom *CA = dyn_cast<ConstAtom>(CV)) {
      this->vars.push_back(CA);
    }
    if (VarAtom *VA = dyn_cast<VarAtom>(CV)) {
      this->vars.push_back(CS.getFreshVar(VA->getName(), VA->getVarKind()));
    }
  }
  if (Ot->FV != nullptr) {
    this->FV = dyn_cast<FVConstraint>(Ot->FV->getCopy(CS));
  }
  this->Parent = Ot;
  this->IsGeneric = Ot->IsGeneric;
  this->IsZeroWidthArray = Ot->IsZeroWidthArray;
  this->OriginallyChecked = Ot->OriginallyChecked;
  // We need not initialize other members.
}

PointerVariableConstraint::PointerVariableConstraint(DeclaratorDecl *D,
                                                     ProgramInfo &I,
                                                     const ASTContext &C) :
        PointerVariableConstraint(D->getType(), D, D->getName(),
                                  I, C) { }

PointerVariableConstraint::PointerVariableConstraint(const QualType &QT,
                                                     DeclaratorDecl *D,
                                                     std::string N,
                                                     ProgramInfo &I,
                                                     const ASTContext &C,
                                                     std::string *inFunc,
                                                     bool Generic) :
        ConstraintVariable(ConstraintVariable::PointerVariable,
                           tyToStr(QT.getTypePtr()),N), FV(nullptr),
        partOFFuncPrototype(inFunc != nullptr), Parent(nullptr),
        IsGeneric(Generic)
{
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

  ArrPresent = false;

  bool isDeclTy = false;
  if (D != nullptr) {
    auto &ABInfo = I.getABoundsInfo();
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

    isDeclTy = D->getType() == QT; // If false, then QT may be D's return type
    if (InteropTypeExpr *ITE = D->getInteropTypeExpr()) {
      // External variables can also have itype.
      // Check if the provided declaration is an external
      // variable.
      // For functions, check to see that if we are analyzing
      // function return types.
      bool AnalyzeITypeExpr = isDeclTy;
      if (!AnalyzeITypeExpr) {
        const Type *OrigType = Ty;
        if (isa<FunctionDecl>(D)) {
          FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
          OrigType = FD->getType().getTypePtr();
        }
        if (OrigType->isFunctionProtoType()) {
          const FunctionProtoType *FPT = dyn_cast<FunctionProtoType>(OrigType);
          AnalyzeITypeExpr = (FPT->getReturnType() == QT);
        }
      }
      if (AnalyzeITypeExpr) {
        QualType InteropType = ITE->getTypeAsWritten();
        QTy = InteropType;
        Ty = QTy.getTypePtr();

        SourceRange R = ITE->getSourceRange();
        if (R.isValid()) {
          ItypeStr = getSourceText(R, C);
          assert(ItypeStr.size() > 0);
        }
      }
    }
  }

  bool VarCreated = false;
  bool IsArr = false;
  bool IsIncompleteArr = false;
  OriginallyChecked = false;
  uint32_t TypeIdx = 0;
  std::string Npre = inFunc ? ((*inFunc)+":") : "";
  VarAtom::VarKind VK =
      inFunc ? (N == RETVAR ? VarAtom::V_Return : VarAtom::V_Param)
             : VarAtom::V_Other;

  while (Ty->isPointerType() || Ty->isArrayType()) {
    // Is this a VarArg type?
    std::string TyName = tyToStr(Ty);
    if (isVarArgType(TyName)) {
      // Variable number of arguments. Make it WILD.
      vars.push_back(CS.getWild());
      VarCreated = true;
      break;
    }

    if (Ty->isCheckedPointerType()) {
      OriginallyChecked = true;
      ConstAtom *CAtom = nullptr;
      if (Ty->isCheckedPointerNtArrayType()) {
        // This is an NT array type.
        CAtom = CS.getNTArr();
      } else if (Ty->isCheckedPointerArrayType()) {
        // This is an array type.
        CAtom = CS.getArr();
      } else if (Ty->isCheckedPointerPtrType()) {
        // This is a regular checked pointer.
        CAtom = CS.getPtr();
      }
      VarCreated = true;
      assert(CAtom != nullptr && "Unable to find the type "
                                 "of the checked pointer.");
      vars.push_back(CAtom);
    }

    if (Ty->isArrayType() || Ty->isIncompleteArrayType()) {
      ArrPresent = IsArr = true;
      IsIncompleteArr = Ty->isIncompleteArrayType();

      // See if there is a constant size to this array type at this position.
      if (const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(Ty)) {
        arrSizes[TypeIdx] = std::pair<OriginalArrType,uint64_t>(
                O_SizedArray,CAT->getSize().getZExtValue());
      } else {
        arrSizes[TypeIdx] = std::pair<OriginalArrType,uint64_t>(
                O_UnSizedArray,0);
      }

      // Boil off the typedefs in the array case.
      while (const TypedefType *TydTy = dyn_cast<TypedefType>(Ty)) {
        QTy = TydTy->desugar();
        Ty = QTy.getTypePtr();
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

      arrSizes[TypeIdx] = std::pair<OriginalArrType,uint64_t>(O_Pointer,0);

      // Iterate.
      QTy = QTy.getSingleStepDesugaredType(C);
      QTy = QTy.getTypePtr()->getPointeeType();
      Ty = QTy.getTypePtr();
    }

    // This type is not a constant atom. We need to create a VarAtom for this.

    if (!VarCreated) {
      VarAtom *VA = CS.getFreshVar(Npre + N, VK);
      vars.push_back(VA);

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
    VK = VarAtom::V_Other; // only the outermost pointer considered a param/return
  }
  insertQualType(TypeIdx, QTy);

  // In CheckedC, a pointer can be freely converted to a size 0 array pointer,
  // but our constraint system does not allow this. To enable converting calls
  // to functions with types similar to free, size 0 array pointers are made PTR
  // instead of ARR.
  IsZeroWidthArray = false;
  if (D && D->hasBoundsExpr() && !vars.empty() && vars[0] == CS.getArr())
    if (BoundsExpr *BE = D->getBoundsExpr())
      if (isZeroBoundsExpr(BE, C)) {
        IsZeroWidthArray = true;
        vars[0] = CS.getPtr();
      }

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
    FV = new FVConstraint(Ty, isDeclTy ? D : nullptr,
                          (IsTypedef ? "" : N), I, C);

  BaseType = tyToStr(Ty);

  bool IsWild = !IsGeneric && (isVarArgType(BaseType) || isTypeHasVoid(QT));
  if (IsWild) {
    std::string Rsn = "Default Var arg list type.";
    if (D && hasVoidType(D))
      Rsn = "Default void* type";
    // TODO: Github issue #61: improve handling of types for
    // Variable arguments.
    for (const auto &V : vars)
      if (VarAtom *VA = dyn_cast<VarAtom>(V))
        CS.addConstraint(CS.createGeq(VA, CS.getWild(), Rsn));
  }

  // Add qualifiers.
  std::ostringstream QualStr;
  getQualString(TypeIdx, QualStr);
  BaseType = QualStr.str() + BaseType;

  // Here lets add implication that if outer pointer is WILD
  // then make the inner pointers WILD too.
  if (vars.size() > 1) {
    bool UsedPrGeq = false;
    for (auto VI=vars.begin(), VE=vars.end(); VI != VE; VI++) {
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

void PointerVariableConstraint::print(raw_ostream &O) const {
  O << "{ ";
  for (const auto &I : vars) {
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

void PointerVariableConstraint::dump_json(llvm::raw_ostream &O) const {
  O << "{\"PointerVar\":{";
  O << "\"Vars\":[";
  bool addComma = false;
  for (const auto &I : vars) {
    if (addComma) {
      O << ",";
    }
    I->dump_json(O);

    addComma = true;
  }
  O << "], \"name\":\"" << getName() << "\"";
  if (FV) {
    O << ", \"FunctionVariable\":";
    FV->dump_json(O);
  }
  O << "}}";

}

void PointerVariableConstraint::getQualString(uint32_t TypeIdx,
                                              std::ostringstream &Ss) {
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

bool PointerVariableConstraint::emitArraySize(std::deque<std::string> &EndStrs,
                                              uint32_t TypeIdx,
                                              bool &EmitName,
                                              bool &EmittedCheckedAnnotation,
                                              bool Nt) {
  bool Ret = false;
  if (ArrPresent) {
    auto i = arrSizes.find(TypeIdx);
    assert(i != arrSizes.end());
    OriginalArrType Oat = i->second.first;
    uint64_t Oas = i->second.second;

    std::ostringstream SizeStr;

    switch (Oat) {
      case O_SizedArray:
        if (!EmittedCheckedAnnotation) {
          SizeStr << (Nt ? " _Nt_checked" : " _Checked");
          //EmittedCheckedAnnotation = true;
        }
        SizeStr << "[" << Oas << "]";
        EndStrs.push_front(SizeStr.str());
        Ret = true;
        break;
      /*case O_UnSizedArray:
        Pss << "[]";
        Ret = true;
        break;*/
      default: break;
    }
    return Ret;
  }
  return Ret;
}

// Mesh resolved constraints with the PointerVariableConstraints set of
// variables and potentially nested function pointer declaration. Produces a
// string that can be replaced in the source code.
std::string
PointerVariableConstraint::mkString(EnvironmentMap &E,
                                    bool EmitName,
                                    bool ForItype,
                                    bool EmitPointee) {
  std::ostringstream Ss;
  std::deque<std::string> EndStrs;
  bool EmittedBase = false;
  bool EmittedName = false;
  bool EmittedCheckedAnnotation = false;
  bool PrevArr = false;
  if ((EmitName == false && hasItype() == false) || getName() == RETVAR)
    EmittedName = true;
  uint32_t TypeIdx = 0;

  auto It = vars.begin();
  // Skip over first pointer level if only emitting pointee string.
  // This is needed when inserting type arguments.
  if (EmitPointee)
    ++It;
  for (; It != vars.end(); ++It) {
    const auto &V = *It;
    ConstAtom *C = nullptr;
    if (ConstAtom *CA = dyn_cast<ConstAtom>(V)) {
      C = CA;
    } else {
      VarAtom *VA = dyn_cast<VarAtom>(V);
      assert(VA != nullptr && "Constraint variable can "
                              "be either constant or VarAtom.");
      C = E[VA].first;
    }
    assert(C != nullptr);

    Atom::AtomKind K = C->getKind();

    // If this is not an itype
    // make this wild as it can hold any pointer type.
    if (!ForItype && BaseType == "void")
      K = Atom::A_Wild;

    if (PrevArr && K != Atom::A_Arr && !EmittedName) {
      EmittedName = true;
      EndStrs.push_front(" " + getName());
    }
    PrevArr = ((K == Atom::A_Arr || K == Atom::A_NTArr)
               && ArrPresent
               && arrSizes[TypeIdx].first == O_SizedArray);

    switch (K) {
      case Atom::A_Ptr:
        getQualString(TypeIdx, Ss);

        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        if (hasItype() == false) {
          EmittedBase = false;
          Ss << "_Ptr<";
          EndStrs.push_front(">");
          break;
        }
        LLVM_FALLTHROUGH;
    case Atom::A_Arr:
        // If this is an array.
        getQualString(TypeIdx, Ss);
        // If it's an Arr, then the character we substitute should
        // be [] instead of *, IF, the original type was an array.
        // And, if the original type was a sized array of size K.
        // we should substitute [K].
        if (emitArraySize(EndStrs, TypeIdx, EmittedName,
                          EmittedCheckedAnnotation, false))
          break;
        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        if (hasItype() == false) {
          EmittedBase = false;
          Ss << "_Array_ptr<";
          EndStrs.push_front(">");
          break;
        }
        LLVM_FALLTHROUGH;
      case Atom::A_NTArr:

        if (emitArraySize(EndStrs, TypeIdx, EmittedName,
                          EmittedCheckedAnnotation, true))
          break;
        // This additional check is to prevent fall-through from the array.
        if (K == Atom::A_NTArr) {
          // If this is an NTArray.
          getQualString(TypeIdx, Ss);

          // We need to check and see if this level of variable
          // is constrained by a bounds safe interface. If it is,
          // then we shouldn't re-write it.
          if (hasItype() == false) {
            EmittedBase = false;
            Ss << "_Nt_array_ptr<";
            EndStrs.push_front(">");
            break;
          }
        }
        LLVM_FALLTHROUGH;
      // If there is no array in the original program, then we fall through to
      // the case where we write a pointer value.
      case Atom::A_Wild:
        if (EmittedBase) {
          Ss << "*";
        } else {
          assert(BaseType.size() > 0);
          EmittedBase = true;
          if (FV) {
            Ss << FV->mkString(E);
          } else {
            Ss << BaseType << " *";
          }
        }

        getQualString(TypeIdx, Ss);
        break;
      case Atom::A_Const:
      case Atom::A_Var:
        llvm_unreachable("impossible");
        break;
    }
    TypeIdx++;
  }

  if (PrevArr && !EmittedName) {
    EmittedName = true;
    EndStrs.push_front(" " + getName());
  }

  if (EmittedBase == false) {
    // If we have a FV pointer, then our "base" type is a function pointer.
    // type.
    if (FV) {
      Ss << FV->mkString(E);
    } else {
      Ss << BaseType;
    }
  }

  // Add closing elements to type
  for (std::string Str : EndStrs) {
    Ss << Str;
  }

  // No space after itype.
  if (!EmittedName)
    Ss << " " << getName();

  //TODO remove comparison to RETVAR
  if (getName() == RETVAR && !ForItype)
    Ss << " ";

  return Ss.str();
}

bool PVConstraint::addArgumentConstraint(ConstraintVariable *DstCons,
                                         ProgramInfo &Info) {
  if (this->Parent == nullptr) {
    bool RetVal = false;
    if (isPartOfFunctionPrototype()) {
      RetVal = argumentConstraints.insert(DstCons).second;
      if (RetVal && this->HasEqArgumentConstraints) {
        constrainConsVarGeq(DstCons, this, Info.getConstraints(), nullptr,
                            Same_to_Same, true, &Info);
      }
    }
    return RetVal;
  }
  return this->Parent->addArgumentConstraint(DstCons, Info);
}

CVarSet &PVConstraint::getArgumentConstraints() {
  return argumentConstraints;
}

FunctionVariableConstraint::
    FunctionVariableConstraint(FunctionVariableConstraint *Ot,
                               Constraints &CS) :
    ConstraintVariable(ConstraintVariable::FunctionVariable,
                       Ot->OriginalType,
                       Ot->getName()) {
  this->IsStatic = Ot->IsStatic;
  this->FileName = Ot->FileName;
  this->Hasbody = Ot->Hasbody;
  this->Hasproto = Ot->Hasproto;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  this->IsFunctionPtr = Ot->IsFunctionPtr;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  // Copy Return CVs.
  for (auto *Rt : Ot->getReturnVars()) {
    this->returnVars.insert(Rt->getCopy(CS));
  }
  // Make copy of ParameterCVs too.
  for (auto &Pset : Ot->paramVars) {
    CVarSet ParmCVs;
    ParmCVs.clear();
    for (auto *ParmPV : Pset) {
      ParmCVs.insert(ParmPV->getCopy(CS));
    }
    this->paramVars.push_back(ParmCVs);
  }
  this->Parent = Ot;
}

// This describes a function, either a function pointer or a function
// declaration itself. Require constraint variables for each argument and
// return, even those that aren't pointer types, since we may need to
// re-emit the function signature as a type.
FunctionVariableConstraint::FunctionVariableConstraint(DeclaratorDecl *D,
                                                       ProgramInfo &I,
                                                       const ASTContext &C) :
        FunctionVariableConstraint(D->getType().getTypePtr(), D,
                                   (D->getDeclName().isIdentifier() ?
                                        D->getName() : ""), I, C)
{ }

FunctionVariableConstraint::FunctionVariableConstraint(const Type *Ty,
                                                       DeclaratorDecl *D,
                                                       std::string N,
                                                       ProgramInfo &I,
                                                       const ASTContext &Ctx) :
        ConstraintVariable(ConstraintVariable::FunctionVariable,
                           tyToStr(Ty), N), Parent(nullptr)
{
  QualType RT;
  Hasproto = false;
  Hasbody = false;
  FileName = "";
  HasEqArgumentConstraints = false;
  IsFunctionPtr = true;

  // Metadata about function
  FunctionDecl *FD = nullptr;
  if (D) FD = dyn_cast<FunctionDecl>(D);
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

  // ConstraintVariables for the parameters
  if (Ty->isFunctionPointerType()) {
    // Is this a function pointer definition?
    llvm_unreachable("should not hit this case");
  } else if (Ty->isFunctionProtoType()) {
    // Is this a function?
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    assert(FT != nullptr);
    RT = FT->getReturnType();

    // Extract the types for the parameters to this function. If the parameter
    // has a bounds expression associated with it, substitute the type of that
    // bounds expression for the other type.
    for (unsigned i = 0; i < FT->getNumParams(); i++) {
      QualType QT = FT->getParamType(i);

      std::string PName = "";
      DeclaratorDecl *ParmVD = nullptr;
      if (FD && i < FD->getNumParams()) {
        ParmVarDecl *PVD = FD->getParamDecl(i);
        if (PVD) {
          ParmVD = PVD;
          PName = PVD->getName();
        }
      }
      bool IsGeneric = ParmVD != nullptr &&
                       getTypeVariableType(ParmVD) != nullptr;
      CVarSet C;
      C.insert(new PVConstraint(QT, ParmVD, PName, I, Ctx, &N, IsGeneric));
      paramVars.push_back(C);
    }

    Hasproto = true;
  } else if (Ty->isFunctionNoProtoType()) {
    const FunctionNoProtoType *FT = Ty->getAs<FunctionNoProtoType>();
    assert(FT != nullptr);
    RT = FT->getReturnType();
  } else {
    llvm_unreachable("don't know what to do");
  }

  // ConstraintVariable for the return
  bool IsGeneric = FD != nullptr && getTypeVariableType(FD) != nullptr;
  returnVars.insert(new PVConstraint(RT, D, RETVAR, I, Ctx, &N, IsGeneric));
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS) {
  for (const auto &V : returnVars)
    V->constrainToWild(CS);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainToWild(CS);
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS,
                                                 const std::string &Rsn) {
  for (const auto &V : returnVars)
    V->constrainToWild(CS, Rsn);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainToWild(CS, Rsn);
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS,
                                                 const std::string &Rsn,
                                                 PersistentSourceLoc *PL) {
  for (const auto &V : returnVars)
    V->constrainToWild(CS, Rsn, PL);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainToWild(CS, Rsn, PL);
}

bool FunctionVariableConstraint::anyChanges(EnvironmentMap &E) {
  bool f = false;

  for (const auto &C : returnVars)
    f |= C->anyChanges(E);

  return f;
}

bool FunctionVariableConstraint::hasWild(EnvironmentMap &E, int AIdx)
{
  for (const auto &C : returnVars)
    if (C->hasWild(E, AIdx))
      return true;

  return false;
}

bool FunctionVariableConstraint::hasArr(EnvironmentMap &E, int AIdx)
{
  for (const auto &C : returnVars)
    if (C->hasArr(E, AIdx))
      return true;

  return false;
}

bool FunctionVariableConstraint::hasNtArr(EnvironmentMap &E, int AIdx)
{
  for (const auto &C : returnVars)
    if (C->hasNtArr(E, AIdx))
      return true;

  return false;
}

ConstraintVariable *FunctionVariableConstraint::getCopy(Constraints &CS) {
  return new FVConstraint(this, CS);
}

void PVConstraint::equateArgumentConstraints(ProgramInfo &Info) {
  if (HasEqArgumentConstraints) {
    return;
  }
  HasEqArgumentConstraints = true;
  for (auto *ArgCons : this->argumentConstraints) {
    constrainConsVarGeq(this, ArgCons, Info.getConstraints(), nullptr,
                        Same_to_Same, true, &Info);
  }

  if (this->FV != nullptr) {
    this->FV->equateArgumentConstraints(Info);
  }
}

void
FunctionVariableConstraint::equateFVConstraintVars(
    CVarSet &Cset, ProgramInfo &Info) {
  for (auto *TmpCons : Cset) {
    if (FVConstraint *FVCons = dyn_cast<FVConstraint>(TmpCons)) {
      for (auto &PConSet : FVCons->paramVars) {
        for (auto *PCon : PConSet) {
          PCon->equateArgumentConstraints(Info);
        }
      }
      for (auto *RCon : FVCons->returnVars) {
        RCon->equateArgumentConstraints(Info);
      }
    }
  }
}

void FunctionVariableConstraint::equateArgumentConstraints(ProgramInfo &Info) {
  if (HasEqArgumentConstraints) {
    return;
  }

  HasEqArgumentConstraints = true;
  CVarSet TmpCSet;
  TmpCSet.insert(this);

  // Equate arguments and parameters vars.
  this->equateFVConstraintVars(TmpCSet, Info);

  // Is this not a function pointer?
  if (!IsFunctionPtr) {
    std::set<FVConstraint *> *DefnCons = nullptr;

    // Get appropriate constraints based on whether the function is static or not.
    if (IsStatic) {
      DefnCons = Info.getStaticFuncConstraintSet(Name, FileName);
    } else {
      DefnCons = Info.getExtFuncDefnConstraintSet(Name);
    }
    assert(DefnCons != nullptr);

    // Equate arguments and parameters vars.
    CVarSet TmpDefn;
    TmpDefn.clear();
    TmpDefn.insert(DefnCons->begin(), DefnCons->end());
    this->equateFVConstraintVars(TmpDefn, Info);
  }
}

void PointerVariableConstraint::constrainToWild(Constraints &CS) {
  ConstAtom *WA = CS.getWild();
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createGeq(VA, WA, true));
  }

  if (FV)
    FV->constrainToWild(CS);
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                const std::string &Rsn,
                                                PersistentSourceLoc *PL) {
  ConstAtom *WA = CS.getWild();
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createGeq(VA, WA, Rsn, PL, true));
  }

  if (FV)
    FV->constrainToWild(CS, Rsn, PL);
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                const std::string &Rsn) {
  ConstAtom *WA = CS.getWild();
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createGeq(VA, WA, Rsn, true));
  }

  if (FV)
    FV->constrainToWild(CS, Rsn);
}

void PointerVariableConstraint::constrainOuterTo(Constraints &CS, ConstAtom *C,
                                                 bool doLB) {
  assert(C == CS.getPtr() || C == CS.getArr() || C == CS.getNTArr());

  if (vars.size() > 0) {
    Atom *A = *vars.begin();
    if (VarAtom *VA = dyn_cast<VarAtom>(A)) {
      if (doLB)
        CS.addConstraint(CS.createGeq(VA, C, false));
      else
        CS.addConstraint(CS.createGeq(C, VA, false));
    } else if (ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
      if (doLB) {
        if (*CA < *C) {
          llvm::errs() << "Warning: " << CA->getStr() << " not less than "
                       << C->getStr() << "\n";
          assert(CA == CS.getWild()); // definitely bogus if not
        }
      }
      else if (*C < *CA) {
        llvm::errs() << "Warning: " << C->getStr() << " not less than " <<
                        CA->getStr() <<"\n";
        assert(CA == CS.getWild()); // definitely bogus if not
      }
    }
  }
}

bool PointerVariableConstraint::anyArgumentIsWild(EnvironmentMap &E) {
  for (auto *ArgVal : argumentConstraints) {
    if (!ArgVal->isChecked(E)) {
      return true;
    }
  }
  return false;
}

bool PointerVariableConstraint::anyChanges(EnvironmentMap &E) {
  // If a pointer variable was checked in the input program, it will have the
  // same checked type in the output, so it cannot have changed.
  if (OriginallyChecked)
    return false;

  // If it was not checked in the input, then it has changed if it now has a
  // checked type.
  bool Ret = false;

  // Are there any non-WILD pointers?
  for (const auto &C : vars) {
    const ConstAtom *CS = getSolution(C, E);
    assert(CS != nullptr && "Atom should be either const or var");
    Ret |= !(isa<WildAtom>(CS));
  }

  if (FV)
    Ret |= FV->anyChanges(E);

  return Ret;
}

ConstraintVariable *PointerVariableConstraint::getCopy(Constraints &CS) {
  return new PointerVariableConstraint(this, CS);
}

const ConstAtom *
PointerVariableConstraint::getSolution(const Atom *A, EnvironmentMap &E) const {
  const ConstAtom *CS = nullptr;
  if (const ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
    CS = CA;
  } else if (const VarAtom *VA = dyn_cast<VarAtom>(A)) {
    // If this is a VarAtom?, we need ot fetch from solution
    // i.e., environment.
    CS = E[const_cast<VarAtom*>(VA)].first;
  }
  assert(CS != nullptr && "Atom should be either const or var");
  return CS;
}

bool PointerVariableConstraint::hasWild(EnvironmentMap &E, int AIdx)
{
  int VarIdx = 0;
  for (const auto &C : vars) {
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

bool PointerVariableConstraint::hasArr(EnvironmentMap &E, int AIdx)
{
  int VarIdx = 0;
  for (const auto &C : vars) {
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

bool PointerVariableConstraint::hasNtArr(EnvironmentMap &E, int AIdx)
{
  int VarIdx = 0;
  for (const auto &C : vars) {
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

bool PointerVariableConstraint::isTopCvarUnsizedArr() {
  if (arrSizes.find(0) != arrSizes.end()) {
    return arrSizes[0].first != O_SizedArray;
  }
  return true;
}

bool PointerVariableConstraint::
    solutionEqualTo(Constraints &CS, ConstraintVariable *CV) {
  bool Ret = false;
  if (CV != nullptr) {
    if (PVConstraint *PV = dyn_cast<PVConstraint>(CV)) {
      auto &OthCVars = PV->vars;
      if (IsGeneric || PV->IsGeneric || vars.size() == OthCVars.size()) {
        Ret = true;

        CAtoms::iterator I = vars.begin();
        CAtoms::iterator J = OthCVars.begin();
        // Special handling for zero width arrays so they can compare equal to
        // ARR or PTR.
        if (IsZeroWidthArray) {
          assert(I != vars.end() && "Zero width array cannot be base type.");
          assert("Zero width arrays should be encoded as PTR."
                     && CS.getAssignment(*I) == CS.getPtr());
          ConstAtom *JAtom = CS.getAssignment(*J);
          // Zero width array can compare as either ARR or PTR
          if (JAtom != CS.getArr() && JAtom != CS.getPtr())
            Ret = false;
          ++I;
          ++J;
        }
        // Compare Vars to see if they are same.
        while (Ret && I != vars.end() && J != OthCVars.end()) {
          if (CS.getAssignment(*I) != CS.getAssignment(*J)) {
            Ret = false;
            break;
          }
          ++I;
          ++J;
        }

        if (Ret) {
          FVConstraint *OtherFV = PV->getFV();
          if (FV != nullptr && OtherFV != nullptr) {
            Ret = FV->solutionEqualTo(CS, OtherFV);
          } else if (FV != nullptr || OtherFV != nullptr) {
            // One of them has FV null.
            Ret = false;
          }
        }
      }
    }
  }
  return Ret;
}

void FunctionVariableConstraint::print(raw_ostream &O) const {
  O << "( ";
  for (const auto &I : returnVars)
    I->print(O);
  O << " )";
  O << " " << Name << " ";
  for (const auto &I : paramVars) {
    O << "( ";
    for (const auto &J : I)
      J->print(O);
    O << " )";
  }
}

void FunctionVariableConstraint::dump_json(raw_ostream &O) const {
  O << "{\"FunctionVar\":{\"ReturnVar\":[";
  bool AddComma = false;
  for (const auto &I : returnVars) {
    if (AddComma) {
      O << ",";
    }
    I->dump_json(O);
  }
  O << "], \"name\":\"" << Name << "\", ";
  O << "\"Parameters\":[";
  AddComma = false;
  for (const auto &I : paramVars) {
    if (I.size() > 0) {
      if (AddComma) {
        O << ",\n";
      }
      O << "[";
      bool InnerComma = false;
      for (const auto &J : I) {
        if (InnerComma) {
          O << ",";
        }
        J->dump_json(O);
        InnerComma = true;
      }
      O << "]";
      AddComma = true;
    }
  }
  O << "]";
  O << "}}";
}

bool FunctionVariableConstraint::hasItype() {
  for (auto &RV : getReturnVars()) {
    if (RV->hasItype()) {
      return true;
    }
  }
  return false;
}

static bool cvSetsSolutionEqualTo(Constraints &CS,
                                CVarSet &CVS1,
                                CVarSet &CVS2) {
  bool Ret = false;
  if (CVS1.size() == CVS2.size()) {
    Ret = CVS1.size() <= 1;
    if (CVS1.size() == 1) {
     auto *CV1 = getOnly(CVS1);
     auto *CV2 = getOnly(CVS2);
     Ret = CV1->solutionEqualTo(CS, CV2);
    }
  }
  return Ret;
}

bool FunctionVariableConstraint::
    solutionEqualTo(Constraints &CS, ConstraintVariable *CV) {
  bool Ret = false;
  if (CV != nullptr) {
    if (FVConstraint *OtherFV = dyn_cast<FVConstraint>(CV)) {
      Ret = (numParams() == OtherFV->numParams());
      Ret = Ret && cvSetsSolutionEqualTo(CS, getReturnVars(),
                                       OtherFV->getReturnVars());
      for (unsigned i=0; i < numParams(); i++) {
        Ret = Ret && cvSetsSolutionEqualTo(CS, getParamVar(i),
                                         OtherFV->getParamVar(i));
      }
    }
  }
  return Ret;
}

std::string
FunctionVariableConstraint::mkString(EnvironmentMap &E,
                                     bool EmitName, bool ForItype,
                                     bool EmitPointee) {
  std::string Ret = "";
  // TODO punting on what to do here. The right thing to do is to figure out
  // The LUB of all of the V in returnVars.
  assert(returnVars.size() > 0);
  ConstraintVariable *V = *returnVars.begin();
  assert(V != nullptr);
  Ret = V->mkString(E);
  Ret = Ret + "(";
  std::vector<std::string> ParmStrs;
  for (const auto &I : this->paramVars) {
    // TODO likewise punting here.
    assert(I.size() > 0);
    ConstraintVariable *U = *(I.begin());
    assert(U != nullptr);
    std::string ParmStr = U->getRewritableOriginalTy() + U->getName();
    if (U->anyChanges(E))
      ParmStr = U->mkString(E);
    ParmStrs.push_back(ParmStr);
  }

  if (ParmStrs.size() > 0) {
    std::ostringstream Ss;

    std::copy(ParmStrs.begin(), ParmStrs.end() - 1,
              std::ostream_iterator<std::string>(Ss, ", "));
    Ss << ParmStrs.back();

    Ret = Ret + Ss.str() + ")";
  } else {
    Ret = Ret + ")";
  }

  return Ret;
}

// Reverses the direction of CA for function subtyping
//   TODO: function pointers forced to be equal right now
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
                          bool doEqType) {
  ConstAtom *CAL, *CAR;
  VarAtom *VAL, *VAR;
  ConstAtom *Wild = CS.getWild();

  CAL = clang::dyn_cast<ConstAtom>(L);
  CAR = clang::dyn_cast<ConstAtom>(R);
  VAL = clang::dyn_cast<VarAtom>(L);
  VAR = clang::dyn_cast<VarAtom>(R);

  // Check constant atom relationships hold
  if (CAR != nullptr && CAL != nullptr) {
    if (doEqType) { // check equality, no matter the atom
      assert(*CAR == *CAL && "Invalid: RHS ConstAtom != LHS ConstAtom");
    } else {
      if (CAL != Wild && CAR != Wild) { // pType atom, disregard CAct
        assert(!(*CAL < *CAR) && "Invalid: LHS ConstAtom < RHS ConstAtom");
      } else { // checked atom (Wild/Ptr); respect CAct
        switch (CAct) {
        case Same_to_Same:
          assert(*CAR == *CAL && "Invalid: RHS ConstAtom != LHS ConstAtom");
          break;
        case Safe_to_Wild:
          assert(!(*CAL < *CAR) && "LHS ConstAtom < RHS ConstAtom");
          break;
        case Wild_to_Safe:
          assert(!(*CAR < *CAL) && "RHS ConstAtom < LHS ConstAtom");
          break;
        }
      }
    }
  } else if (VAL != nullptr && VAR != nullptr) {
    switch (CAct) {
    case Same_to_Same:
      // Equality for checked.
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
      // Not for ptyp.
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      // Unless indicated.
      if (doEqType)
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      break;
    case Safe_to_Wild:
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      if (doEqType) {
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      }
      break;
    case Wild_to_Safe:
      // Note: reversal.
      CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      if (doEqType) {
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
        if (doEqType)
          CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        break;
      case Wild_to_Safe:
	CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true)); // note reversal!
        if (doEqType)
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
        if (doEqType)
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
                         ConsAction CA, bool doEqType, ProgramInfo *Info) {

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
        // FIXME: Make neg(CA) here? Function pointers equated
        constrainConsVarGeq(FCLHS->getReturnVars(), FCRHS->getReturnVars(), CS,
                            PL, Same_to_Same, doEqType, Info);

        // Constrain the parameters contravariantly.
        if (FCLHS->numParams() == FCRHS->numParams()) {
          for (unsigned i = 0; i < FCLHS->numParams(); i++) {
            CVarSet &LHSV =
                FCLHS->getParamVar(i);
            CVarSet &RHSV =
                FCRHS->getParamVar(i);
            // FIXME: Make neg(CA) here? Now: Function pointers equated
            constrainConsVarGeq(RHSV, LHSV, CS, PL, Same_to_Same, doEqType,
                                Info);
          }
        } else {
          // Constrain both to be top.
          std::string Rsn = "Assigning from:" + FCRHS->getName() +
                            " to " + FCLHS->getName();
          RHS->constrainToWild(CS, Rsn, PL);
          LHS->constrainToWild(CS, Rsn, PL);
        }
      } else {
        llvm_unreachable("impossible");
      }
    }
    else if (PVConstraint *PCLHS = dyn_cast<PVConstraint>(LHS)) {
      if (PVConstraint *PCRHS = dyn_cast<PVConstraint>(RHS)) {

        // Add assignment to bounds info graph.
        if (PCLHS->hasBoundsKey() && PCRHS->hasBoundsKey()) {
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
          if (CLHS.size() == CRHS.size()
              || PCLHS->getIsGeneric() || PCRHS->getIsGeneric()) {
            unsigned Max = std::max(CLHS.size(), CRHS.size());
            for (unsigned N = 0; N < Max; N++) {
              Atom *IAtom = PCLHS->getAtom(N, CS);
              Atom *JAtom = PCRHS->getAtom(N, CS);
              if (IAtom == nullptr || JAtom == nullptr)
                break;

              // Get outermost pointer first, using current ConsAction.
              if (N == 0)
                createAtomGeq(CS, IAtom, JAtom, Rsn, PL, CA, doEqType);
              else {
                // Now constrain the inner ones as equal.
                createAtomGeq(CS, IAtom, JAtom, Rsn, PL, CA, true);
              }
            }
          // Unequal sizes means casting from (say) T** to T*; not safe.
          // unless assigning to a generic type.
          } else {
            // Constrain both to be top.
            std::string Rsn = "Assigning from:" + std::to_string(CRHS.size())
                              + " depth pointer to " +
                              std::to_string(CLHS.size()) + " depth pointer.";
            PCLHS->constrainToWild(CS, Rsn, PL);
            PCRHS->constrainToWild(CS, Rsn, PL);
          }
          // Equate the corresponding FunctionConstraint.
          constrainConsVarGeq(PCLHS->getFV(), PCRHS->getFV(), CS, PL, CA,
                              doEqType, Info);
        }
      } else
        llvm_unreachable("impossible");
    } else
      llvm_unreachable("unknown kind");
  }
  else {
    // Assigning from a function variable to a pointer variable?
    PVConstraint *PCLHS = dyn_cast<PVConstraint>(LHS);
    FVConstraint *FCRHS = dyn_cast<FVConstraint>(RHS);
    if (PCLHS && FCRHS) {
      if (FVConstraint *FCLHS = PCLHS->getFV()) {
        constrainConsVarGeq(FCLHS, FCRHS, CS, PL, CA, doEqType, Info);
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

// Given an RHS and a LHS, constrain them to be equal.
void constrainConsVarGeq(CVarSet &LHS,
                      CVarSet &RHS,
                      Constraints &CS,
                      PersistentSourceLoc *PL,
                      ConsAction CA,
                      bool doEqType,
                      ProgramInfo *Info) {
  for (const auto &I : LHS) {
    for (const auto &J : RHS) {
      constrainConsVarGeq(I, J, CS, PL, CA, doEqType, Info);
    }
  }
}

// True if [C] is a PVConstraint that contains at least one Atom (i.e.,
//   it represents a C pointer).
bool isAValidPVConstraint(ConstraintVariable *C) {
  if (C != nullptr) {
    if (PVConstraint *PV = dyn_cast<PVConstraint>(C))
      return !PV->getCvars().empty();
  }
  return false;
}

// Replace CVars and argumentConstraints with those in [FromCV].
void PointerVariableConstraint::brainTransplant(ConstraintVariable *FromCV,
                                                ProgramInfo &I) {
  PVConstraint *From = dyn_cast<PVConstraint>(FromCV);
  assert (From != nullptr);
  CAtoms CFrom = From->getCvars();
  assert (vars.size() == CFrom.size());
  if (From->hasBoundsKey()) {
    // If this has bounds key!? Then do brain transplant of
    // bound keys as well.
    if (hasBoundsKey())
      I.getABoundsInfo().brainTransplant(getBoundsKey(),
                                         From->getBoundsKey());

    ValidBoundsKey = From->hasBoundsKey();
    BKey = From->getBoundsKey();
  }
  vars = CFrom; // FIXME: structural copy? By reference?
  argumentConstraints = From->getArgumentConstraints();
  if (FV) {
    assert(From->FV);
    FV->brainTransplant(From->FV, I);
  }
}

void PointerVariableConstraint::mergeDeclaration(ConstraintVariable *FromCV,
                                                 ProgramInfo &Info) {
  PVConstraint *From = dyn_cast<PVConstraint>(FromCV);
  std::vector<Atom *> NewVatoms;
  CAtoms CFrom = From->getCvars();
  CAtoms::iterator I = vars.begin();
  CAtoms::iterator J = CFrom.begin();
  while (I != vars.end()) {
    Atom *IAt = *I;
    Atom *JAt = *J;
    ConstAtom *ICAt = dyn_cast<ConstAtom>(IAt);
    ConstAtom *JCAt = dyn_cast<ConstAtom>(JAt);
    if (JCAt && !ICAt) {
      NewVatoms.push_back(JAt);
    } else {
      NewVatoms.push_back(IAt);
    }
    if (ICAt && JCAt) {
      // Both are ConstAtoms, no need to equate them.

      // Sanity: If both are ConstAtoms and they are not same,
      // Make sure that current ConstAtom is WILD. This ensure that
      // we are moving towards checked types.
      if (ICAt != JCAt) {
        if (!dyn_cast<WildAtom>(ICAt)) {
          assert(false && "Should be same checked types");
        }
      }
    }
    ++I;
    ++J;
  }
  assert (vars.size() == NewVatoms.size() && "Merging Failed");
  vars = NewVatoms;
  if (!From->ItypeStr.empty())
    ItypeStr = From->ItypeStr;
  if (FV) {
    assert(From->FV);
    FV->mergeDeclaration(From->FV, Info);
  }
}

Atom *PointerVariableConstraint::getAtom(unsigned AtomIdx, Constraints &CS) {
  if (AtomIdx < vars.size()) {
    // If index is in bounds, just return the atom.
    return vars[AtomIdx];
  } else if (IsGeneric && AtomIdx == vars.size()) {
    // Polymorphic types don't know how "deep" their pointers are beforehand so,
    // we need to create new atoms for new pointer levels on the fly.
    std::string Stars(vars.size(), '*');
    Atom *A = CS.getFreshVar(Name + Stars, VarAtom::V_Other);
    vars.push_back(A);
    return A;
  }
  return nullptr;
}

// Brain Transplant params and returns in [FromCV], recursively.
void FunctionVariableConstraint::brainTransplant(ConstraintVariable *FromCV,
                                                 ProgramInfo &I) {
  FVConstraint *From = dyn_cast<FVConstraint>(FromCV);
  assert (From != nullptr);
  // Transplant returns.
  auto FromRetVar = getOnly(From->getReturnVars());
  auto RetVar = getOnly(returnVars);
  RetVar->brainTransplant(FromRetVar, I);
  // Transplant params.
  if (numParams() == From->numParams()) {
    for (unsigned i = 0; i < From->numParams(); i++) {
      CVarSet &FromP = From->getParamVar(i);
      CVarSet &P = getParamVar(i);
      auto FromVar = getOnly(FromP);
      auto Var = getOnly(P);
      Var->brainTransplant(FromVar, I);
    }
  } else if (numParams() != 0 && From->numParams() == 0) {
    auto &CS = I.getConstraints();
    std::vector<ParamDeferment> &defers = From->getDeferredParams();
    assert(getDeferredParams().size() == 0);
    for (auto deferred : defers ) {
      assert(numParams() == deferred.PS.size());
      for(unsigned i = 0; i < deferred.PS.size(); i++) {
        CVarSet ParamDC = getParamVar(i);
        CVarSet ArgDC = deferred.PS[i];
        constrainConsVarGeq(ParamDC, ArgDC, CS, &(deferred.PL), Wild_to_Safe, false, &I);
      }
    }
  } else {
    llvm_unreachable("Brain Transplant on empty params");
  }
}

void FunctionVariableConstraint::mergeDeclaration(ConstraintVariable *FromCV,
                                                  ProgramInfo &I) {
  // `this`: is the declaration the tool saw first
  // `FromCV`: is the declaration seen second, it cannot have defered constraints
  FVConstraint *From = dyn_cast<FVConstraint>(FromCV);
  assert(From != nullptr);
  assert(From->getDeferredParams().size() == 0);
  // Transplant returns.
  auto FromRetVar = getOnly(From->getReturnVars());
  auto RetVar = getOnly(returnVars);
  RetVar->mergeDeclaration(FromRetVar, I);

  if (From->numParams() == 0) {
    // From is an untyped declaration, and adds no information
    return;
  } else if (this->numParams() == 0) {
    // This is an untyped declaration, we need to perform a transplant
    From->brainTransplant(this, I);
  } else {
    // Standard merge
    assert(this->numParams() == From->numParams());
    for (unsigned i = 0; i < From->numParams(); i++) {
      CVarSet &FromP = From->getParamVar(i);
      auto FromVar = getOnly(FromP);
      CVarSet &P = getParamVar(i);
      auto Var = getOnly(P);
      Var->mergeDeclaration(FromVar, I);
    }
  }
}


void FunctionVariableConstraint::addDeferredParams
(PersistentSourceLoc PL, std::vector<CVarSet> Ps) {
  ParamDeferment P = { PL, Ps };
  deferredParams.push_back(P);
}


bool FunctionVariableConstraint::getIsOriginallyChecked() {
  for (const auto &R : returnVars)
    if (R->getIsOriginallyChecked())
      return true;
  return false;
}
