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

ConstraintVariable *
ConstraintVariable::getHighestNonWildConstraint(std::set<ConstraintVariable *>
                                                &ToCheck,
                                                EnvironmentMap &E,
                                                ProgramInfo &I) {
  ConstraintVariable *HighestConVar = nullptr;
  for (auto CurrCons : ToCheck) {
    // If the current constraint is not WILD.
    if (!CurrCons->hasWild(E)) {
      if (HighestConVar == nullptr)
        HighestConVar = CurrCons;
      else if (HighestConVar->isLt(*CurrCons, I))
        HighestConVar = CurrCons;
    }
  }
  return HighestConVar;
}

PointerVariableConstraint *PointerVariableConstraint::GlobalWildPV = nullptr;

PointerVariableConstraint *
PointerVariableConstraint::getWildPVConstraint(Constraints &CS) {
  if (GlobalWildPV == nullptr) {
    // Is this the first time? Then create PVConstraint.
    CAtoms NewVA;
    NewVA.push_back(CS.getWild());
    GlobalWildPV =
        new PVConstraint(NewVA, "unsigned", "var", nullptr, false, false, "");
  }
  return GlobalWildPV;
}

PointerVariableConstraint::
    PointerVariableConstraint(PointerVariableConstraint *Ot,
                              Constraints &CS) :
    ConstraintVariable(ConstraintVariable::PointerVariable,
                       Ot->BaseType, Ot->Name),
    FV(nullptr), partOFFuncPrototype(Ot->partOFFuncPrototype) {
  this->arrSizes = Ot->arrSizes;
  this->ArrPresent = Ot->ArrPresent;
  this->HasDefDeclEquated = Ot->HasDefDeclEquated;
  // Make copy of the vars only for VarAtoms.
  for (auto *CV : Ot->vars) {
    if (ConstAtom *CA = dyn_cast<ConstAtom>(CV)) {
      this->vars.push_back(CA);
    }
    if (VarAtom *VA = dyn_cast<VarAtom>(CV)) {
      this->vars.push_back(CS.getFreshVar(VA->getName()));
    }
  }
  if (Ot->FV != nullptr) {
    this->FV = dyn_cast<FVConstraint>(Ot->FV->getCopy(CS));
  }
  this->Parent = Ot;
  // We need not initialize other members.
}

PointerVariableConstraint::PointerVariableConstraint(DeclaratorDecl *D,
                                                     Constraints &CS,
                                                     const ASTContext &C) :
        PointerVariableConstraint(D->getType(), D, D->getName(),
                                  CS, C) { }

PointerVariableConstraint::PointerVariableConstraint(const QualType &QT,
                                                     DeclaratorDecl *D,
                                                     std::string N,
                                                     Constraints &CS,
                                                     const ASTContext &C,
                                                     std::string *inFunc) :
        ConstraintVariable(ConstraintVariable::PointerVariable,
                           tyToStr(QT.getTypePtr()),N), FV(nullptr),
        partOFFuncPrototype(inFunc != nullptr), Parent(nullptr)
{
  QualType QTy = QT;
  const Type *Ty = QTy.getTypePtr();
  OriginalType = tyToStr(Ty);
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

  if (InteropTypeExpr *ITE = D->getInteropTypeExpr()) {
    // External variables can also have itype.
    // Check if the provided declaration is an external
    // variable.
    QualType InteropType = ITE->getTypeAsWritten();
    QTy = InteropType;
    Ty = QTy.getTypePtr();

    SourceRange R = ITE->getSourceRange();
    if (R.isValid()) {
      auto &SM = C.getSourceManager();
      auto LO = C.getLangOpts();
      llvm::StringRef Srctxt =
              Lexer::getSourceText(CharSourceRange::getTokenRange(R), SM, LO);
      ItypeStr = Srctxt.str();
      assert(ItypeStr.size() > 0);
    }
  }

  bool VarCreated = false;
  uint32_t TypeIdx = 0;
  std::string Npre = inFunc ? ((*inFunc)+":") : "";
  while (Ty->isPointerType() || Ty->isArrayType()) {
    VarCreated = false;
    // Is this a VarArg type?
    std::string TyName = tyToStr(Ty);
    // TODO: Github issue #61: improve handling of types for
    // // Variable arguments.
    if (isVarArgType(TyName)) {
      // Variable number of arguments. Make it WILD.
      vars.push_back(CS.getWild());
      VarCreated = true;
      break;
    }

    if (Ty->isDeclaredCheckedPointerType()) {
      ConstAtom *CAtom = nullptr;
      if (Ty->isDeclaredCheckedPointerNtArrayType()) {
        // This is an NT array type.
        CAtom = CS.getNTArr();
      } else if (Ty->isDeclaredCheckedPointerArrayType()) {
        // This is an array type.
        CAtom = CS.getArr();
      } else if (Ty->isDeclaredCheckedPointerPtrType()) {
        // This is a regular checked pointer.
        CAtom = CS.getPtr();
      }
      VarCreated = true;
      assert(CAtom != nullptr && "Unable to find the type "
                                 "of the checked pointer.");
      vars.push_back(CAtom);
    }

    if (Ty->isArrayType() || Ty->isIncompleteArrayType()) {
      ArrPresent = true;
      // If it's an array, then we need both a constraint variable
      // for each level of the array, and a constraint variable for
      // values stored in the array.

      // See if there is a constant size to this array type at this position.
      if (const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(Ty)) {
        arrSizes[TypeIdx] = std::pair<OriginalArrType,uint64_t>(
                O_SizedArray,CAT->getSize().getZExtValue());
        if (AllTypes && !VarCreated) {
          // This is a statically declared array. Make it a Checked Array.
          vars.push_back(CS.getArr());
          VarCreated = true;
        }
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
      if (QTy.isConstQualified())
        QualMap.insert(
                std::pair<uint32_t, Qualification>(TypeIdx,
                                                    ConstQualification));

      arrSizes[TypeIdx] = std::pair<OriginalArrType,uint64_t>(O_Pointer,0);

      // Iterate.
      QTy = QTy.getSingleStepDesugaredType(C);
      QTy = QTy.getTypePtr()->getPointeeType();
      Ty = QTy.getTypePtr();
    }

    // This type is not a constant atom. We need to create a VarAtom for this.
    if (!VarCreated) {
      vars.push_back(CS.getFreshVar(Npre+N));
    }
    TypeIdx++;
    Npre = Npre + "*";
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
    FV = new FVConstraint(Ty, D, (IsTypedef ? "" : N), CS, C);

  BaseType = tyToStr(Ty);

  bool IsWild = isVarArgType(BaseType) || isTypeHasVoid(QT);
  if (IsWild) {
    std::string Rsn = "Default Var arg list type.";
    if (hasVoidType(D))
      Rsn = "Default void* type";
    // TODO: Github issue #61: improve handling of types for
    // Variable arguments.
    for (const auto &V : vars)
      if (VarAtom *VA = dyn_cast<VarAtom>(V))
        CS.addConstraint(CS.createGeq(VA, CS.getWild(), Rsn));
  }

  // Add qualifiers.
  if (QTy.isConstQualified()) {
    BaseType = "const " + BaseType;
  }

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

bool PVConstraint::liftedOnCVars(const ConstraintVariable &O,
                                 ProgramInfo &Info,
                                 llvm::function_ref<bool (ConstAtom *,
                                                         ConstAtom *)> Op) const
{
  // If these aren't both PVConstraints, incomparable.
  if (!isa<PVConstraint>(O))
    return false;

  const PVConstraint *P = cast<PVConstraint>(&O);
  const CAtoms &OC = P->getCvars();

  // If they don't have the same number of cvars, incomparable.
  if (OC.size() != getCvars().size())
    return false;

  auto I = getCvars().begin();
  auto J = OC.begin();
  Constraints &CS = Info.getConstraints();
  auto &Env = CS.getVariables();

  while (I != getCvars().end() && J != OC.end()) {
    // Look up the valuation for I and J.
    ConstAtom *CI = const_cast<ConstAtom*>(getPtrSolution(*I, Env));
    ConstAtom *CJ = const_cast<ConstAtom*>(getPtrSolution(*J, Env));

    if (!Op(CI, CJ))
      return false;

    ++I;
    ++J;
  }

  return true;
}

bool PVConstraint::isLt(const ConstraintVariable &Other,
                        ProgramInfo &Info) const
{
  if (isEmpty() || Other.isEmpty())
    return false;

  return liftedOnCVars(Other, Info, [](ConstAtom *A, ConstAtom *B) {
      return *A < *B;
  });
}

bool PVConstraint::isEq(const ConstraintVariable &Other,
                        ProgramInfo &Info) const
{
  if (isEmpty() && Other.isEmpty())
    return true;

  if (isEmpty() || Other.isEmpty())
    return false;

  return liftedOnCVars(Other, Info, [](ConstAtom *A, ConstAtom *B) {
      return *A == *B;
  });
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
  std::map<ConstraintKey, Qualification>::iterator Q = QualMap.find(TypeIdx);
  if (Q != QualMap.end())
    if (Q->second == ConstQualification)
      Ss << "const ";
}

bool PointerVariableConstraint::emitArraySize(std::ostringstream &Pss,
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

    if (EmitName == false) {
      EmitName = true;
      Pss << getName();
    }

    switch (Oat) {
      case O_SizedArray:
        if (!EmittedCheckedAnnotation) {
          Pss << (Nt ? " _Nt_checked" : " _Checked");
          EmittedCheckedAnnotation = true;
        }
        Pss << "[" << Oas << "]";
        Ret = true;
        break;
      case O_UnSizedArray:
        Pss << "[]";
        Ret = true;
        break;
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
                                    bool ForItype) {
  std::ostringstream Ss;
  std::ostringstream Pss;
  unsigned CaratsToAdd = 0;
  bool EmittedBase = false;
  bool EmittedName = false;
  bool EmittedCheckedAnnotation = false;
  if (EmitName == false && getItypePresent() == false)
    EmittedName = true;
  uint32_t TypeIdx = 0;
  for (const auto &V : vars) {
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

    switch (K) {
      case Atom::A_Ptr:
        getQualString(TypeIdx, Ss);

        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        if (getItypePresent() == false) {
          EmittedBase = false;
          Ss << "_Ptr<";
          CaratsToAdd++;
          break;
        }
      case Atom::A_Arr:
        // If this is an array.
        getQualString(TypeIdx, Ss);
        // If it's an Arr, then the character we substitute should
        // be [] instead of *, IF, the original type was an array.
        // And, if the original type was a sized array of size K.
        // we should substitute [K].
        if (emitArraySize(Pss, TypeIdx, EmittedName,
                          EmittedCheckedAnnotation, false))
          break;
        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        if (getItypePresent() == false) {
          EmittedBase = false;
          Ss << "_Array_ptr<";
          CaratsToAdd++;
          break;
        }
      case Atom::A_NTArr:

        if (emitArraySize(Pss, TypeIdx, EmittedName,
                          EmittedCheckedAnnotation, true))
          break;
        // This additional check is to prevent fall-through from the array.
        if (K == Atom::A_NTArr) {
          // If this is an NTArray.
          getQualString(TypeIdx, Ss);

          // We need to check and see if this level of variable
          // is constrained by a bounds safe interface. If it is,
          // then we shouldn't re-write it.
          if (getItypePresent() == false) {
            EmittedBase = false;
            Ss << "_Nt_array_ptr<";
            CaratsToAdd++;
            break;
          }
        }
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
            Ss << BaseType << "*";
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

  if (EmittedBase == false) {
    // If we have a FV pointer, then our "base" type is a function pointer.
    // type.
    if (FV) {
      Ss << FV->mkString(E);
    } else {
      Ss << BaseType;
    }
  }

  // Push carats onto the end of the string.
  for (unsigned i = 0; i < CaratsToAdd; i++) {
    Ss << ">";
  }

  // No space after itype.
  if (!ForItype)
    Ss << " ";

  std::string FinalDec;
  if (EmittedName == false) {
    if (getName() != RETVAR)
      Ss << getName();
    FinalDec = Ss.str();
  } else {
    FinalDec = Ss.str() + Pss.str();
  }

  return FinalDec;
}

bool PVConstraint::addArgumentConstraint(ConstraintVariable *DstCons,
                                         ProgramInfo &Info) {
  if (this->Parent == nullptr) {
    bool RetVal = false;
    if (isPartOfFunctionPrototype()) {
      RetVal = argumentConstraints.insert(DstCons).second;
      if (RetVal && this->HasDefDeclEquated) {
        constrainConsVarGeq(DstCons, this, Info.getConstraints(),
                            nullptr,Same_to_Same, true, &Info);
      }
    }
    return RetVal;
  }
  return this->Parent->addArgumentConstraint(DstCons, Info);
}

std::set<ConstraintVariable *> &PVConstraint::getArgumentConstraints() {
  return argumentConstraints;
}

FunctionVariableConstraint::
    FunctionVariableConstraint(FunctionVariableConstraint *Ot,
                               Constraints &CS) :
    ConstraintVariable(ConstraintVariable::FunctionVariable,
                       Ot->BaseType,
                       Ot->getName()) {
  this->IsStatic = Ot->IsStatic;
  this->FileName = Ot->FileName;
  this->Hasbody = Ot->Hasbody;
  this->Hasproto = Ot->Hasproto;
  this->name = Ot->name;
  this->HasDefDeclEquated = Ot->HasDefDeclEquated;
  this->IsFunctionPtr = Ot->IsFunctionPtr;
  this->HasDefDeclEquated = Ot->HasDefDeclEquated;
  // Copy Return CVs.
  for (auto *Rt : Ot->getReturnVars()) {
    this->returnVars.insert(Rt->getCopy(CS));
  }
  // Make copy of ParameterCVs too.
  for (auto &Pset : Ot->paramVars) {
    std::set<ConstraintVariable *> ParmCVs;
    ParmCVs.clear();
    for (auto *ParmPV : Pset) {
      ParmCVs.insert(ParmPV->getCopy(CS));
    }
    this->paramVars.push_back(ParmCVs);
  }
  this->Parent = Ot;
}

// This describes a function, either a function pointer or a function
// declaration itself. Either require constraint variables for any pointer
// types that are either return values or paraemeters for the function.
FunctionVariableConstraint::FunctionVariableConstraint(DeclaratorDecl *D,
                                                       Constraints &CS,
                                                       const ASTContext &C) :
        FunctionVariableConstraint(D->getType().getTypePtr(), D,
                                   (D->getDeclName().isIdentifier() ?
                                        D->getName() : ""), CS, C)
{ }

FunctionVariableConstraint::FunctionVariableConstraint(const Type *Ty,
                                                       DeclaratorDecl *D,
                                                       std::string N,
                                                       Constraints &CS,
                                                       const ASTContext &Ctx) :
        ConstraintVariable(ConstraintVariable::FunctionVariable,
                           tyToStr(Ty), N),
        name(N), Parent(nullptr)
{
  QualType RT;
  Hasproto = false;
  Hasbody = false;
  FileName = "";
  HasDefDeclEquated = false;
  IsFunctionPtr = true;

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    // FunctionDecl::hasBody will return true if *any* declaration in the
    // declaration chain has a body, which is not what we want to record.
    // We want to record if *this* declaration has a body. To do that,
    // we'll check if the declaration that has the body is different
    // from the current declaration.
    const FunctionDecl *OFd = nullptr;
    if (FD->hasBody(OFd) && OFd == FD)
      Hasbody = true;
    IsStatic = !(FD->isGlobal());
    ASTContext *TmpCtx = const_cast<ASTContext*>(&Ctx);
    auto PSL = PersistentSourceLoc::mkPSL(D, *TmpCtx);
    FileName = PSL.getFileName();
    IsFunctionPtr = false;
  }

  if (Ty->isFunctionPointerType()) {
    // Is this a function pointer definition?
    llvm_unreachable("should not hit this case");
  } else if (Ty->isFunctionProtoType()) {
    // Is this a function?
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
    assert(FT != nullptr);
    RT = FT->getReturnType();

    // Extract the types for the parameters to this function. If the parameter
    // has a bounds expression associated with it, substitute the type of that
    // bounds expression for the other type.
    for (unsigned i = 0; i < FT->getNumParams(); i++) {
      QualType QT = FT->getParamType(i);

      if (InteropTypeExpr *BA =  FT->getParamAnnots(i).getInteropTypeExpr()) {
        QualType InteropType= Ctx.getInteropTypeAndAdjust(BA, true);
        // TODO: handle array_ptr types.
        if (InteropType->isCheckedPointerPtrType())
          QT = InteropType;
      }

      std::string PName = "";
      DeclaratorDecl *TmpD = D;
      if (FD && i < FD->getNumParams()) {
        ParmVarDecl *PVD = FD->getParamDecl(i);
        if (PVD) {
          TmpD = PVD;
          PName = PVD->getName();
        }
      }

      std::set<ConstraintVariable *> C;
      C.insert(new PVConstraint(QT, TmpD, PName, CS, Ctx, &N));
      paramVars.push_back(C);
    }

    if (InteropTypeExpr *BA = FT->getReturnAnnots().getInteropTypeExpr()) {
      QualType InteropType = Ctx.getInteropTypeAndAdjust(BA, false);
      // TODO: handle array_ptr types.
      if (InteropType->isCheckedPointerPtrType())
        RT = InteropType;
    }
    Hasproto = true;
  } else if (Ty->isFunctionNoProtoType()) {
    const FunctionNoProtoType *FT = Ty->getAs<FunctionNoProtoType>();
    assert(FT != nullptr);
    RT = FT->getReturnType();
  } else {
    llvm_unreachable("don't know what to do");
  }
  // This has to be a mapping for all parameter/return types, even those that
  // aren't pointer types. If we need to re-emit the function signature
  // as a type, then we will need the types for all the parameters and the
  // return values.

  returnVars.insert(new PVConstraint(RT, D, RETVAR, CS, Ctx, &N));
  std::string Rsn = "Function pointer return value.";
  for ( const auto &V : returnVars) {
    if (PVConstraint *PVC = dyn_cast<PVConstraint>(V)) {
      if (PVC->getFV())
        PVC->constrainToWild(CS, Rsn, false);
    } else if (FVConstraint *FVC = dyn_cast<FVConstraint>(V)) {
      FVC->constrainToWild(CS, Rsn, false);
    }
  }
}

bool FVConstraint::liftedOnCVars(const ConstraintVariable &Other,
                                 ProgramInfo &Info,
                                 llvm::function_ref<bool (ConstAtom *,
                                                         ConstAtom *)> Op) const
{
  if (!isa<FVConstraint>(Other))
    return false;

  const FVConstraint *F = cast<FVConstraint>(&Other);

  if (paramVars.size() != F->paramVars.size()) {
    if (paramVars.size() < F->paramVars.size()) {
      return true;
    } else {
      return false;
    }
  }

  // Consider the return variables.
  ConstraintVariable *U = getHighest(returnVars, Info);
  ConstraintVariable *V = getHighest(F->returnVars, Info);

  if (!U->liftedOnCVars(*V, Info, Op))
    return false;

  // Consider the parameters.
  auto I = paramVars.begin();
  auto J = F->paramVars.begin();

  while ((I != paramVars.end()) && (J != F->paramVars.end())) {
    U = getHighest(*I, Info);
    V = getHighest(*J, Info);

    if (!U->liftedOnCVars(*V, Info, Op))
      return false;

    ++I;
    ++J;
  }

  return true;
}

bool FVConstraint::isLt(const ConstraintVariable &Other,
                        ProgramInfo &Info) const
{
  if (isEmpty() || Other.isEmpty())
    return false;

  return liftedOnCVars(Other, Info, [](ConstAtom *A, ConstAtom *B) {
      return *A < *B;
  });
}

bool FVConstraint::isEq(const ConstraintVariable &Other,
                        ProgramInfo &Info) const
{
  if (isEmpty() && Other.isEmpty())
    return true;

  if (isEmpty() || Other.isEmpty())
    return false;

  return liftedOnCVars(Other, Info, [](ConstAtom *A, ConstAtom *B) {
      return *A == *B;
  });
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS,
                                                 bool CheckSkip) {
  for (const auto &V : returnVars)
    V->constrainToWild(CS, CheckSkip);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainToWild(CS, CheckSkip);
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS,
                                                 std::string &Rsn,
                                                 bool CheckSkip) {
  for (const auto &V : returnVars)
    V->constrainToWild(CS, Rsn, CheckSkip);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainToWild(CS, Rsn, CheckSkip);
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS,
                                                 std::string &Rsn,
                                                 PersistentSourceLoc *PL,
                                                 bool CheckSkip) {
  for (const auto &V : returnVars)
    V->constrainToWild(CS, Rsn, PL, CheckSkip);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainToWild(CS, Rsn, PL, CheckSkip);
}

bool FunctionVariableConstraint::anyChanges(EnvironmentMap &E) {
  bool f = false;

  for (const auto &C : returnVars)
    f |= C->anyChanges(E);

  return f;
}

bool FunctionVariableConstraint::hasWild(EnvironmentMap &E)
{
  for (const auto &C : returnVars)
    if (C->hasWild(E))
      return true;

  return false;
}

bool FunctionVariableConstraint::hasArr(EnvironmentMap &E)
{
  for (const auto &C : returnVars)
    if (C->hasArr(E))
      return true;

  return false;
}

bool FunctionVariableConstraint::hasNtArr(EnvironmentMap &E)
{
  for (const auto &C : returnVars)
    if (C->hasNtArr(E))
      return true;

  return false;
}

ConstraintVariable *FunctionVariableConstraint::getCopy(Constraints &CS) {
  return new FVConstraint(this, CS);
}

ConstAtom*
FunctionVariableConstraint::getHighestType(EnvironmentMap &E) {
  ConstAtom *Ret = nullptr;
  for (const auto &C : returnVars) {
    ConstAtom *CS = C->getHighestType(E);
    assert(CS != nullptr);
    if (Ret == nullptr || ((*Ret) < *CS)) {
      Ret = CS;
    }
  }
  return Ret;
}

void PVConstraint::equateInsideOutsideVars(ProgramInfo &Info) {
  if (HasDefDeclEquated) {
    return;
  }
  HasDefDeclEquated = true;
  for (auto *ArgCons : this->argumentConstraints) {
    constrainConsVarGeq(this, ArgCons, Info.getConstraints(), nullptr,
                        Same_to_Same, true, &Info);
  }

  if (this->FV != nullptr) {
    this->FV->equateInsideOutsideVars(Info);
  }
}

void
FunctionVariableConstraint::equateFVConstraintVars(
    std::set<ConstraintVariable *> &Cset, ProgramInfo &Info) {
  for (auto *TmpCons : Cset) {
    if (FVConstraint *FVCons = dyn_cast<FVConstraint>(TmpCons)) {
      for (auto &PConSet : FVCons->paramVars) {
        for (auto *PCon : PConSet) {
          PCon->equateInsideOutsideVars(Info);
        }
      }
      for (auto *RCon : FVCons->returnVars) {
        RCon->equateInsideOutsideVars(Info);
      }
    }
  }
}

void FunctionVariableConstraint::equateInsideOutsideVars(ProgramInfo &Info) {
  std::set<FVConstraint *> *DeclCons = nullptr;
  std::set<FVConstraint *> *DefnCons = nullptr;

  if (HasDefDeclEquated) {
    return;
  }

  HasDefDeclEquated = true;
  std::set<ConstraintVariable *> TmpCSet;
  TmpCSet.insert(this);

  // Equate arguments and parameters vars.
  this->equateFVConstraintVars(TmpCSet, Info);

  // Is this not a function pointer?
  if (!IsFunctionPtr) {
    // Get appropriate constraints based on whether the function is static or not.
    if (IsStatic) {
      DeclCons = Info.getStaticFuncDeclConstraintSet(name, FileName);
      DefnCons = Info.getStaticFuncDefnConstraintSet(name, FileName);
    } else {
      DeclCons = Info.getExtFuncDeclConstraintSet(name);
      DefnCons = Info.getExtFuncDefnConstraintSet(name);
    }

    // Only when we have both declaration and definition constraints.
    // Then equate them.
    if (DefnCons != nullptr && DeclCons != nullptr) {
      std::set<ConstraintVariable *> TmpDecl, TmpDefn;
      TmpDecl.clear();
      TmpDefn.clear();

      TmpDecl.insert(DeclCons->begin(), DeclCons->end());
      TmpDefn.insert(DefnCons->begin(), DefnCons->end());
      // Equate declaration and definition constraint
      //   (Need to call twice to unify at all levels)
      constrainConsVarGeq(TmpDecl, TmpDefn, Info.getConstraints(), nullptr,
                          Same_to_Same, true, &Info);
      constrainConsVarGeq(TmpDefn, TmpDecl, Info.getConstraints(), nullptr,
                          Same_to_Same, true, &Info);

      // Equate arguments and parameters vars.
      this->equateFVConstraintVars(TmpDecl, Info);
      this->equateFVConstraintVars(TmpDefn, Info);
    }
  }
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                bool CheckSkip) {
  ConstAtom *WA = CS.getWild();
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createGeq(VA, WA, true));
  }

  if (FV)
    FV->constrainToWild(CS, CheckSkip);
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                std::string &Rsn,
                                                PersistentSourceLoc *PL,
                                                bool CheckSkip) {
  ConstAtom *WA = CS.getWild();
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createGeq(VA, WA, Rsn, PL, true));
  }

  if (FV)
    FV->constrainToWild(CS, Rsn, PL, CheckSkip);
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                std::string &Rsn,
                                                bool CheckSkip) {
  ConstAtom *WA = CS.getWild();
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createGeq(VA, WA, Rsn, true));
  }

  if (FV)
    FV->constrainToWild(CS, Rsn, CheckSkip);
}

// FIXME: Should do some checking here, eventually to make sure
// checked types are respected
void PointerVariableConstraint::constrainOuterTo(Constraints &CS, ConstAtom *C) {
  assert(C == CS.getPtr() || C == CS.getArr() || C == CS.getNTArr());

  if (vars.size() > 0) {
    Atom *A = *vars.begin();
    if (VarAtom *VA = dyn_cast<VarAtom>(A))
      CS.addConstraint(CS.createGeq(C, VA, false));
    else if (ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
      assert (!(*C < *CA));
    }
  }
}

bool PointerVariableConstraint::anyArgumentIsWild(EnvironmentMap &E) {
  for (auto *ArgVal : argumentConstraints) {
    if (!(ArgVal->anyChanges(E))) {
      return true;
    }
  }
  return false;
}

bool PointerVariableConstraint::anyChanges(EnvironmentMap &E) {
  bool Ret = false;

  // Are there any non-WILD pointers?
  for (const auto &C : vars) {
    const ConstAtom *CS = getPtrSolution(C, E);
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

const ConstAtom*
PointerVariableConstraint::getPtrSolution(const Atom *A,
                                          EnvironmentMap &E) const{
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

bool PointerVariableConstraint::hasWild(EnvironmentMap &E)
{
  for (const auto &C : vars) {
    const ConstAtom *CS = getPtrSolution(C, E);
    if (isa<WildAtom>(CS))
      return true;
  }

  if (FV)
    return FV->hasWild(E);

  return false;
}

bool PointerVariableConstraint::hasArr(EnvironmentMap &E)
{
  for (const auto &C : vars) {
    const ConstAtom *CS = getPtrSolution(C, E);
    if (isa<ArrAtom>(CS))
      return true;
  }

  if (FV)
    return FV->hasArr(E);

  return false;
}

bool PointerVariableConstraint::hasNtArr(EnvironmentMap &E)
{
  for (const auto &C : vars) {
    const ConstAtom *CS = getPtrSolution(C, E);
    if (isa<NTArrAtom>(CS))
      return true;
  }

  if (FV)
    return FV->hasNtArr(E);

  return false;
}

ConstAtom*
PointerVariableConstraint::getHighestType(EnvironmentMap &E) {
  ConstAtom *Ret = nullptr;
  for (const auto &C : vars) {
    const ConstAtom *CS = getPtrSolution(C, E);
    if (Ret == nullptr || ((*Ret) < *CS)) {
      Ret = const_cast<ConstAtom*>(CS);
    }
  }
  return Ret;
}

void FunctionVariableConstraint::print(raw_ostream &O) const {
  O << "( ";
  for (const auto &I : returnVars)
    I->print(O);
  O << " )";
  O << " " << name << " ";
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
  O << "], \"name\":\"" << name << "\", ";
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

std::string
FunctionVariableConstraint::mkString(EnvironmentMap &E,
                                     bool EmitName, bool ForItype) {
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

static ConsAction neg(ConsAction CA) {
  switch (CA) {
  case Safe_to_Wild: return Wild_to_Safe;
  case Wild_to_Safe: return Safe_to_Wild;
  case Same_to_Same: return Same_to_Same;
  }
  // Silencing the compiler.
  assert(false && "Can never reach here.");
  return Same_to_Same;
}

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
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true)); // Equality for checked
      CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false)); // Not for ptyp ...
      if (doEqType)
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false)); // .... Unless indicated
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
      CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true)); // note reversal!
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      if (doEqType) {
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      }
      break;
    }
  } else {
    if (CAL == Wild || CAR == Wild) { // This should be a checked/unchecked constraint
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
    } else { // This should be a pointer-type constraint
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
// If doEqType is true, then also do CA |- LHS <: RHS
void constrainConsVarGeq(ConstraintVariable *LHS,
                      ConstraintVariable *RHS,
                      Constraints &CS,
                      PersistentSourceLoc *PL,
                      ConsAction CA,
                      bool doEqType,
                      ProgramInfo *I) {

  // If one of the constraint is NULL, make the other constraint WILD.
  // This can happen when a non-function pointer gets assigned to
  // a function pointer.
  if (LHS == nullptr || RHS == nullptr) {
    std::string Rsn = "Assignment a non-function pointer "
                      "to a function pointer";
    if (LHS != nullptr) {
      LHS->constrainToWild(CS, Rsn, PL, false);
    }
    if (RHS != nullptr) {
      RHS->constrainToWild(CS, Rsn, PL, false);
    }
    return;
  }

  if (RHS->getKind() == LHS->getKind()) {
    if (FVConstraint *FCLHS = dyn_cast<FVConstraint>(LHS)) {
      if (FVConstraint *FCRHS = dyn_cast<FVConstraint>(RHS)) {

        // This is an assignment between function pointer and
        // function pointer or a function.
        // Equate the definition and declaration.
        FCLHS->equateInsideOutsideVars(*I);
        FCRHS->equateInsideOutsideVars(*I);

        // Constrain the return values covariantly.
        constrainConsVarGeq(FCLHS->getReturnVars(), FCRHS->getReturnVars(), CS,
                            PL, Same_to_Same, true, I);

        // Constrain the parameters contravariantly
        if (FCLHS->numParams() == FCRHS->numParams()) {
          for (unsigned i = 0; i < FCLHS->numParams(); i++) {
            std::set<ConstraintVariable *> &LHSV =
                FCLHS->getParamVar(i);
            std::set<ConstraintVariable *> &RHSV =
                FCRHS->getParamVar(i);
            constrainConsVarGeq(RHSV, LHSV, CS, PL, Same_to_Same, doEqType, I);
          }
        } else {
          // Constrain both to be top.
          std::string Rsn = "Assigning from:" + FCRHS->getName() +
                            " to " + FCLHS->getName();
          RHS->constrainToWild(CS, Rsn, PL, false);
          LHS->constrainToWild(CS, Rsn, PL, false);
        }
      } else {
        llvm_unreachable("impossible");
      }
    }
    else if (PVConstraint *PCLHS = dyn_cast<PVConstraint>(LHS)) {
      if (PVConstraint *PCRHS = dyn_cast<PVConstraint>(RHS)) {
        std::string Rsn = "";
        // This is to handle function subtyping. Try to add LHS and RHS
        // to each others argument constraints.
        PCLHS->addArgumentConstraint(PCRHS, *I);
        PCRHS->addArgumentConstraint(PCLHS, *I);
        // Element-wise constrain PCLHS and PCRHS to be equal
        CAtoms CLHS = PCLHS->getCvars();
        CAtoms CRHS = PCRHS->getCvars();
        if (CLHS.size() == CRHS.size()) {
          int n = 0;
          CAtoms::iterator I = CLHS.begin();
          CAtoms::iterator J = CRHS.begin();
          while (I != CLHS.end()) {
	    // Get outermost pointer first, using current ConsAction
            if (n == 0) createAtomGeq(CS, *I, *J, Rsn, PL, CA, doEqType);
            else {
	      // Now constrain the inner ones as equal
	      createAtomGeq(CS, *I, *J, Rsn, PL, CA, true);
	    }
            ++I;
            ++J;
            n++;
          }
        } else {
          // Constrain both to be top.
          std::string Rsn = "Assigning from:" + PCRHS->getName() +
                            " to " + PCLHS->getName();
          PCLHS->constrainToWild(CS, Rsn, PL, false);
          PCRHS->constrainToWild(CS, Rsn, PL, false);
        }
        // Equate the corresponding FunctionContraint.
        constrainConsVarGeq(PCLHS->getFV(), PCRHS->getFV(), CS, PL,
                            CA, doEqType, I);
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
        constrainConsVarGeq(FCLHS, FCRHS, CS, PL, CA, doEqType, I);
      } else {
          std::string Rsn = "Function:" + FCRHS->getName() +
                            " assigned to non-function pointer.";
          LHS->constrainToWild(CS, Rsn, PL, false);
          RHS->constrainToWild(CS, Rsn, PL, false);
      }
    } else {
      // Constrain everything in both to wild.
      std::string Rsn = "Assignment to functions from variables";
      LHS->constrainToWild(CS, Rsn, PL, false);
      RHS->constrainToWild(CS, Rsn, PL, false);
    }
  }
}

// Given an RHS and a LHS, constrain them to be equal.
void constrainConsVarGeq(std::set<ConstraintVariable *> &LHS,
                      std::set<ConstraintVariable *> &RHS,
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

