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
                                                Constraints::EnvironmentMap &E,
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

PointerVariableConstraint::PointerVariableConstraint(DeclaratorDecl *D,
                                                     ConstraintKey &K,
                                                     Constraints &CS,
                                                     const ASTContext &C) :
        PointerVariableConstraint(D->getType(), K, D, D->getName(),
                                  CS, C) { }

PointerVariableConstraint::PointerVariableConstraint(const QualType &QT,
                                                     ConstraintKey &K,
                                                     DeclaratorDecl *D,
                                                     std::string N,
                                                     Constraints &CS,
                                                     const ASTContext &C,
                                                     bool PartOfFunc) :
        ConstraintVariable(ConstraintVariable::PointerVariable,
                           tyToStr(QT.getTypePtr()),N), FV(nullptr),
        partOFFuncPrototype(PartOfFunc)
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
    if (!dyn_cast<ParmVarDecl>(D) && !dyn_cast<FunctionDecl>(D)) {
      QualType InteropType = C.getInteropTypeAndAdjust(ITE, false);
      // TODO: handle array_ptr types.
      if (InteropType->isCheckedPointerPtrType()) {
        QTy = InteropType;
        Ty = QTy.getTypePtr();
      }
    }

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
    if (Ty->isArrayType() || Ty->isIncompleteArrayType()) {
      ArrPresent = true;
      // If it's an array, then we need both a constraint variable
      // for each level of the array, and a constraint variable for
      // values stored in the array.

      // See if there is a constant size to this array type at this position.
      if (const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(Ty)) {
        arrSizes[TypeIdx] = std::pair<OriginalArrType,uint64_t>(
                O_SizedArray,CAT->getSize().getZExtValue());
        if (AllTypes) {
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
      assert(CS.getVar(K) == nullptr);
      vars.push_back(CS.getOrCreateVar(K));
      K++;
    }
    TypeIdx++;
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
    FV = new FVConstraint(Ty, K, D, (IsTypedef ? "" : N), CS, C);

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
        CS.addConstraint(CS.createEq(VA, CS.getWild(), Rsn));
  }

  // Add qualifiers.
  if (QTy.isConstQualified()) {
    BaseType = "const " + BaseType;
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
PointerVariableConstraint::mkString(Constraints::EnvironmentMap &E,
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
      C = E[VA];
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
    Ss << getName();
    FinalDec = Ss.str();
  } else {
    FinalDec = Ss.str() + Pss.str();
  }

  return FinalDec;
}

bool PVConstraint::addArgumentConstraint(ConstraintVariable *DstCons) {
  if (isPartOfFunctionPrototype())
    return argumentConstraints.insert(DstCons).second;

  return false;
}
std::set<ConstraintVariable *> &PVConstraint::getArgumentConstraints() {
  return argumentConstraints;
}

// This describes a function, either a function pointer or a function
// declaration itself. Either require constraint variables for any pointer
// types that are either return values or paraemeters for the function.
FunctionVariableConstraint::FunctionVariableConstraint(DeclaratorDecl *D,
                                                       ConstraintKey &K,
                                                       Constraints &CS,
                                                       const ASTContext &C) :
        FunctionVariableConstraint(D->getType().getTypePtr(), K, D,
                                   (D->getDeclName().isIdentifier() ?
                                        D->getName() : ""), CS, C)
{ }

FunctionVariableConstraint::FunctionVariableConstraint(const Type *Ty,
                                                       ConstraintKey &K,
                                                       DeclaratorDecl *D,
                                                       std::string N,
                                                       Constraints &CS,
                                                       const ASTContext &Ctx) :
        ConstraintVariable(ConstraintVariable::FunctionVariable,
                           tyToStr(Ty), N),
        name(N)
{
  QualType RT;
  Hasproto = false;
  Hasbody = false;

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    // FunctionDecl::hasBody will return true if *any* declaration in the
    // declaration chain has a body, which is not what we want to record.
    // We want to record if *this* declaration has a body. To do that,
    // we'll check if the declaration that has the body is different
    // from the current declaration.
    const FunctionDecl *OFd = nullptr;
    if (FD->hasBody(OFd) && OFd == FD)
      Hasbody = true;
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
      C.insert(new PVConstraint(QT, K, TmpD, PName, CS, Ctx, true));
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

  returnVars.insert(new PVConstraint(RT, K, D, "", CS, Ctx, true));
  std::string Rsn = "Function pointer return value.";
  for ( const auto &V : returnVars) {
    if (PVConstraint *PVC = dyn_cast<PVConstraint>(V)) {
      if (PVC->getFV())
        PVC->constrainTo(CS, CS.getWild(), Rsn);
    } else if (FVConstraint *FVC = dyn_cast<FVConstraint>(V)) {
      FVC->constrainTo(CS, CS.getWild(), Rsn);
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

void FunctionVariableConstraint::constrainTo(Constraints &CS, ConstAtom *A,
                                             bool CheckSkip) {
  for (const auto &V : returnVars)
    V->constrainTo(CS, A, CheckSkip);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainTo(CS, A, CheckSkip);
}


void FunctionVariableConstraint::constrainTo(Constraints &CS, ConstAtom *A,
                                             std::string &Rsn, bool CheckSkip) {
  for (const auto &V : returnVars)
    V->constrainTo(CS, A, Rsn, CheckSkip);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainTo(CS, A, Rsn, CheckSkip);
}

void FunctionVariableConstraint::constrainTo(Constraints &CS, ConstAtom *C,
                                             std::string &Rsn,
                                             PersistentSourceLoc *PL,
                                             bool CheckSkip) {
  for (const auto &V : returnVars)
    V->constrainTo(CS, C, Rsn, PL, CheckSkip);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainTo(CS, C, Rsn, PL, CheckSkip);
}

bool FunctionVariableConstraint::anyChanges(Constraints::EnvironmentMap &E) {
  bool f = false;

  for (const auto &C : returnVars)
    f |= C->anyChanges(E);

  return f;
}

bool FunctionVariableConstraint::hasWild(Constraints::EnvironmentMap &E)
{
  for (const auto &C : returnVars)
    if (C->hasWild(E))
      return true;

  return false;
}

bool FunctionVariableConstraint::hasArr(Constraints::EnvironmentMap &E)
{
  for (const auto &C : returnVars)
    if (C->hasArr(E))
      return true;

  return false;
}

bool FunctionVariableConstraint::hasNtArr(Constraints::EnvironmentMap &E)
{
  for (const auto &C : returnVars)
    if (C->hasNtArr(E))
      return true;

  return false;
}

ConstAtom*
FunctionVariableConstraint::getHighestType(Constraints::EnvironmentMap &E) {
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

bool PointerVariableConstraint::canConstraintCKey(Constraints &CS,
                                                  ConstraintKey Ck,
                                                  ConstAtom *CA,
                                                  bool CheckSkip) {
  // Check and see if we've already constrained this variable. This is currently
  // only done when the bounds-safe interface has refined a type for an external
  // function, and we don't want the linking phase to un-refine it by introducing
  // a conflicting constraint.
  bool Add = true;
  // This will ensure that we do not make an itype constraint
  // variable to be WILD (which should be impossible)!!.
  if (CheckSkip || dyn_cast<WildAtom>(CA)) {
    if (ConstrainedVars.find(Ck) != ConstrainedVars.end())
      Add = false;
  }
  // See, if we can constrain the current constraint var to the provided
  // ConstAtom.
  if (!CS.getOrCreateVar(Ck)->canAssign(CA))
    Add = false;

  return Add;
}

void PointerVariableConstraint::constrainTo(Constraints &CS, ConstAtom *A,
                                            bool CheckSkip) {
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createEq(VA, A));
  }

  if (FV)
    FV->constrainTo(CS, A, CheckSkip);
}

void PointerVariableConstraint::constrainTo(Constraints &CS, ConstAtom *C,
                                            std::string &Rsn,
                                            PersistentSourceLoc *PL,
                                            bool CheckSkip) {
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createEq(VA, C, Rsn, PL));
  }

  if (FV)
    FV->constrainTo(CS, C, Rsn, PL, CheckSkip);
}

void PointerVariableConstraint::constrainTo(Constraints &CS, ConstAtom *A,
                                            std::string &Rsn, bool CheckSkip) {
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createEq(VA, A, Rsn));
  }

  if (FV)
    FV->constrainTo(CS, A, Rsn, CheckSkip);
}
bool PointerVariableConstraint::anyChanges(Constraints::EnvironmentMap &E) {
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

const ConstAtom*
PointerVariableConstraint::getPtrSolution(const Atom *A,
                                          Constraints::EnvironmentMap &E) const{
  const ConstAtom *CS = nullptr;
  if (const ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
    CS = CA;
  } else if (const VarAtom *VA = dyn_cast<VarAtom>(A)) {
    // If this is a VarAtom?, we need ot fetch from solution
    // i.e., environment.
    CS = E[const_cast<VarAtom*>(VA)];
  }
  assert(CS != nullptr && "Atom should be either const or var");
  return CS;
}

bool PointerVariableConstraint::hasWild(Constraints::EnvironmentMap &E)
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

bool PointerVariableConstraint::hasArr(Constraints::EnvironmentMap &E)
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

bool PointerVariableConstraint::hasNtArr(Constraints::EnvironmentMap &E)
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
PointerVariableConstraint::getHighestType(Constraints::EnvironmentMap &E) {
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
FunctionVariableConstraint::mkString(Constraints::EnvironmentMap &E,
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
