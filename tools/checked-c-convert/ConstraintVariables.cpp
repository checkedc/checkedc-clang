//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of ConstraintVariables methods.
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringSwitch.h"
#include "clang/Lex/Lexer.h"
#include <sstream>

#include "ConstraintVariables.h"
#include "ProgramInfo.h"

using namespace clang;

// Helper method to print a Type in a way that can be represented in the source.
static
std::string
tyToStr(const Type *T) {
  QualType QT(T, 0);

  return QT.getAsString();
}

PointerVariableConstraint::PointerVariableConstraint(DeclaratorDecl *D,
                                                     ConstraintKey &K, Constraints &CS, const ASTContext &C) :
        PointerVariableConstraint(D->getType(), K, D, D->getName(), CS, C) { }

PointerVariableConstraint::PointerVariableConstraint(const QualType &QT, ConstraintKey &K,
                                                     DeclaratorDecl *D, std::string N, Constraints &CS, const ASTContext &C) :
        ConstraintVariable(ConstraintVariable::PointerVariable,
                           tyToStr(QT.getTypePtr()),N),FV(nullptr)
{
  QualType QTy = QT;
  const Type *Ty = QTy.getTypePtr();
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

  bool isTypedef = false;

  if (Ty->getAs<TypedefType>())
    isTypedef = true;

  arrPresent = false;

  if (InteropTypeExpr *ITE = D->getInteropTypeExpr()) {
    SourceRange R = ITE->getSourceRange();
    if (R.isValid()) {
      auto &SM = C.getSourceManager();
      auto LO = C.getLangOpts();
      llvm::StringRef txt =
              Lexer::getSourceText(CharSourceRange::getTokenRange(R), SM, LO);
      itypeStr = txt.str();
      assert(itypeStr.size() > 0);
    }
  }

  while (Ty->isPointerType() || Ty->isArrayType()) {
    if (Ty->isArrayType() || Ty->isIncompleteArrayType()) {
      arrPresent = true;
      // If it's an array, then we need both a constraint variable
      // for each level of the array, and a constraint variable for
      // values stored in the array.
      vars.insert(K);
      assert(CS.getVar(K) == nullptr);
      CS.getOrCreateVar(K);

      // See if there is a constant size to this array type at this position.
      if (const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(Ty)) {
        arrSizes[K] = std::pair<OriginalArrType,uint64_t>(
                O_SizedArray,CAT->getSize().getZExtValue());
      } else {
        arrSizes[K] = std::pair<OriginalArrType,uint64_t>(
                O_UnSizedArray,0);
      }

      K++;

      // Boil off the typedefs in the array case.
      while(const TypedefType *tydTy = dyn_cast<TypedefType>(Ty)) {
        QTy = tydTy->desugar();
        Ty = QTy.getTypePtr();
      }

      // Iterate.
      if(const ArrayType *arrTy = dyn_cast<ArrayType>(Ty)) {
        QTy = arrTy->getElementType();
        Ty = QTy.getTypePtr();
      } else {
        llvm_unreachable("unknown array type");
      }
    } else {
      // Allocate a new constraint variable for this level of pointer.
      vars.insert(K);
      assert(CS.getVar(K) == nullptr);
      VarAtom * V = CS.getOrCreateVar(K);

      if (Ty->isCheckedPointerType()) {
        if (Ty->isCheckedPointerNtArrayType()) {
          // this is an NT array type
          // Constrain V to be not equal to Arr, Ptr or Wild
          CS.addConstraint(CS.createNot(CS.createEq(V, CS.getArr())));
          CS.addConstraint(CS.createNot(CS.createEq(V, CS.getPtr())));
          CS.addConstraint(CS.createNot(CS.createEq(V, CS.getWild())));
          ConstrainedVars.insert(K);
        } else if (Ty->isCheckedPointerArrayType()) {
          // this is an array type
          // Constrain V to be not equal to NTArr, Ptr or Wild
          CS.addConstraint(CS.createNot(CS.createEq(V, CS.getNTArr())));
          CS.addConstraint(CS.createNot(CS.createEq(V, CS.getPtr())));
          CS.addConstraint(CS.createNot(CS.createEq(V, CS.getWild())));
          ConstrainedVars.insert(K);
        } else if (Ty->isCheckedPointerPtrType()) {
          // Constrain V so that it can't be either wild or an array or an NTArray
          CS.addConstraint(CS.createNot(CS.createEq(V, CS.getArr())));
          CS.addConstraint(CS.createNot(CS.createEq(V, CS.getNTArr())));
          CS.addConstraint(CS.createNot(CS.createEq(V, CS.getWild())));
          ConstrainedVars.insert(K);
        }
      }

      // Save here if QTy is qualified or not into a map that
      // indexes K to the qualification of QTy, if any.
      if (QTy.isConstQualified())
        QualMap.insert(
                std::pair<ConstraintKey, Qualification>(K, ConstQualification));

      arrSizes[K] = std::pair<OriginalArrType,uint64_t>(O_Pointer,0);

      K++;
      std::string TyName = tyToStr(Ty);
      // TODO: Github issue #61: improve handling of types for
      // // variable arguments.
      if (TyName == "struct __va_list_tag *" || TyName == "va_list")
        break;

      // Iterate.
      QTy = QTy.getSingleStepDesugaredType(C);
      QTy = QTy.getTypePtr()->getPointeeType();
      Ty = QTy.getTypePtr();
    }
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
    FV = new FVConstraint(Ty, K, D, (isTypedef ? "" : N), CS, C);

  BaseType = tyToStr(Ty);

  if (QTy.isConstQualified()) {
    BaseType = "const " + BaseType;
  }

  // TODO: Github issue #61: improve handling of types for
  // variable arguments.
  if (BaseType == "struct __va_list_tag *" || BaseType == "va_list" ||
      BaseType == "struct __va_list_tag")
    for (const auto &V : vars)
      CS.addConstraint(CS.createEq(CS.getOrCreateVar(V), CS.getWild()));
}

bool PVConstraint::liftedOnCVars(const ConstraintVariable &O,
                                 ProgramInfo &Info,
                                 llvm::function_ref<bool (ConstAtom *, ConstAtom *)> Op) const
{
  // If these aren't both PVConstraints, incomparable.
  if (!isa<PVConstraint>(O))
    return false;

  const PVConstraint *P = cast<PVConstraint>(&O);
  const CVars &OC = P->getCvars();

  // If they don't have the same number of cvars, incomparable.
  if (OC.size() != getCvars().size())
    return false;

  auto I = getCvars().begin();
  auto J = OC.begin();
  Constraints &CS = Info.getConstraints();
  auto env = CS.getVariables();

  while(I != getCvars().end() && J != OC.end()) {
    // Look up the valuation for I and J.
    ConstAtom *CI = env[CS.getVar(*I)];
    ConstAtom *CJ = env[CS.getVar(*J)];

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
  for (const auto &I : vars)
    O << "q_" << I << " ";
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
    if(addComma) {
      O << ",";
    }
    O << "\"q_" << I << "\"";
    addComma = true;
  }
  O << "], \"name\":\"" << getName() << "\"";
  if(FV) {
    O << ", \"FunctionVariable\":";
    FV->dump_json(O);
  }
  O << "}}";

}

void PointerVariableConstraint::getQualString(ConstraintKey targetCVar, std::ostringstream &ss) {
  std::map<ConstraintKey, Qualification>::iterator q = QualMap.find(targetCVar);
  if (q != QualMap.end())
    if (q->second == ConstQualification)
      ss << "const ";
}

// Mesh resolved constraints with the PointerVariableConstraints set of
// variables and potentially nested function pointer declaration. Produces a
// string that can be replaced in the source code.
std::string
PointerVariableConstraint::mkString(Constraints::EnvironmentMap &E, bool emitName, bool forItype) {
  std::ostringstream ss;
  std::ostringstream pss;
  unsigned caratsToAdd = 0;
  bool emittedBase = false;
  bool emittedName = false;
  if (emitName == false && getItypePresent() == false)
    emittedName = true;
  for (const auto &V : vars) {
    VarAtom VA(V);
    ConstAtom *C = E[&VA];
    assert(C != nullptr);

    Atom::AtomKind K = C->getKind();

    // if this is not an itype
    // make this wild as it can hold any pointer type
    if (!forItype && BaseType == "void")
      K = Atom::A_Wild;

    switch (K) {
      case Atom::A_Ptr:
        getQualString(V, ss);

        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        if (getItypePresent() == false) {
          emittedBase = false;
          ss << "_Ptr<";
          caratsToAdd++;
          break;
        }
      case Atom::A_Arr:
        // If it's an Arr, then the character we substitute should
        // be [] instead of *, IF, the original type was an array.
        // And, if the original type was a sized array of size K,
        // we should substitute [K].
        if (arrPresent) {
          auto i = arrSizes.find(V);
          assert(i != arrSizes.end());
          OriginalArrType oat = i->second.first;
          uint64_t oas = i->second.second;

          if (emittedName == false) {
            emittedName = true;
            pss << getName();
          }

          switch(oat) {
            case O_Pointer:
              pss << "*";
              break;
            case O_SizedArray:
              pss << "[" << oas << "]";
              break;
            case O_UnSizedArray:
              pss << "[]";
              break;
          }

          break;
        }
      case Atom::A_NTArr:
        // if this is an NTArray
        getQualString(V, ss);

        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        if (getItypePresent() == false) {
          emittedBase = false;
          ss << "_Nt_arr_ptr<";
          caratsToAdd++;
          break;
        }
        // If there is no array in the original program, then we fall through to
        // the case where we write a pointer value.
      case Atom::A_Wild:
        if (emittedBase) {
          ss << "*";
        } else {
          assert(BaseType.size() > 0);
          emittedBase = true;
          if (FV) {
            ss << FV->mkString(E);
          } else {
            ss << BaseType << "*";
          }
        }

        getQualString(V, ss);
        break;
      case Atom::A_Const:
      case Atom::A_Var:
        llvm_unreachable("impossible");
        break;
    }
  }

  if(emittedBase == false) {
    // If we have a FV pointer, then our "base" type is a function pointer
    // type.
    if (FV) {
      ss << FV->mkString(E);
    } else {
      ss << BaseType;
    }
  }

  // Push carats onto the end of the string
  for (unsigned i = 0; i < caratsToAdd; i++) {
    ss << ">";
  }

  ss << " ";

  std::string finalDec;
  if (emittedName == false) {
    ss << getName();
    finalDec = ss.str();
  } else {
    finalDec = ss.str() + pss.str();
  }

  return finalDec;
}

// This describes a function, either a function pointer or a function
// declaration itself. Either require constraint variables for any pointer
// types that are either return values or paraemeters for the function.
FunctionVariableConstraint::FunctionVariableConstraint(DeclaratorDecl *D,
                                                       ConstraintKey &K, Constraints &CS, const ASTContext &C) :
        FunctionVariableConstraint(D->getType().getTypePtr(), K, D,
                                   (D->getDeclName().isIdentifier() ? D->getName() : ""), CS, C)
{ }

FunctionVariableConstraint::FunctionVariableConstraint(const Type *Ty,
                                                       ConstraintKey &K, DeclaratorDecl *D, std::string N, Constraints &CS, const ASTContext &Ctx) :
        ConstraintVariable(ConstraintVariable::FunctionVariable, tyToStr(Ty), N),name(N)
{
  QualType returnType;
  hasproto = false;
  hasbody = false;

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    // FunctionDecl::hasBody will return true if *any* declaration in the
    // declaration chain has a body, which is not what we want to record.
    // We want to record if *this* declaration has a body. To do that,
    // we'll check if the declaration that has the body is different
    // from the current declaration.
    const FunctionDecl *oFD = nullptr;
    if (FD->hasBody(oFD) && oFD == FD)
      hasbody = true;
  }

  if (Ty->isFunctionPointerType()) {
    // Is this a function pointer definition?
    llvm_unreachable("should not hit this case");
  } else if (Ty->isFunctionProtoType()) {
    // Is this a function?
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
    assert(FT != nullptr);
    returnType = FT->getReturnType();

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

      std::string paramName = "";
      DeclaratorDecl *tmpD = D;
      if (FD && i < FD->getNumParams()) {
        ParmVarDecl *PVD = FD->getParamDecl(i);
        if (PVD) {
          tmpD = PVD;
          paramName = PVD->getName();
        }
      }

      std::set<ConstraintVariable*> C;
      C.insert(new PVConstraint(QT, K, tmpD, paramName, CS, Ctx));
      paramVars.push_back(C);
    }

    if (InteropTypeExpr *BA = FT->getReturnAnnots().getInteropTypeExpr()) {
      QualType InteropType = Ctx.getInteropTypeAndAdjust(BA, false);
      // TODO: handle array_ptr types.
      if (InteropType->isCheckedPointerPtrType())
        returnType = InteropType;
    }
    hasproto = true;
  } else if (Ty->isFunctionNoProtoType()) {
    const FunctionNoProtoType *FT = Ty->getAs<FunctionNoProtoType>();
    assert(FT != nullptr);
    returnType = FT->getReturnType();
  } else {
    llvm_unreachable("don't know what to do");
  }
  // This has to be a mapping for all parameter/return types, even those that
  // aren't pointer types. If we need to re-emit the function signature
  // as a type, then we will need the types for all the parameters and the
  // return values

  returnVars.insert(new PVConstraint(returnType, K, D, "", CS, Ctx));
  for ( const auto &V : returnVars) {
    if (PVConstraint *PVC = dyn_cast<PVConstraint>(V)) {
      if (PVC->getFV())
        PVC->constrainTo(CS, CS.getWild());
    } else if (FVConstraint *FVC = dyn_cast<FVConstraint>(V)) {
      FVC->constrainTo(CS, CS.getWild());
    }
  }
}

bool FVConstraint::liftedOnCVars(const ConstraintVariable &Other,
                                 ProgramInfo &Info,
                                 llvm::function_ref<bool (ConstAtom *, ConstAtom *)> Op) const
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

void FunctionVariableConstraint::constrainTo(Constraints &CS, ConstAtom *A, bool checkSkip) {
  for (const auto &V : returnVars)
    V->constrainTo(CS, A, checkSkip);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainTo(CS, A, checkSkip);
}

bool FunctionVariableConstraint::anyChanges(Constraints::EnvironmentMap &E) {
  bool f = false;

  for (const auto &C : returnVars)
    f |= C->anyChanges(E);

  return f;
}

bool FunctionVariableConstraint::hasWild(Constraints::EnvironmentMap &E)
{
  for (const auto& C: returnVars)
    if (C->hasWild(E))
      return true;

  return false;
}

bool FunctionVariableConstraint::hasArr(Constraints::EnvironmentMap &E)
{
  for (const auto& C: returnVars)
    if (C->hasArr(E))
      return true;

  return false;
}

void PointerVariableConstraint::constrainTo(Constraints &CS, ConstAtom *A, bool checkSkip) {
  for (const auto &V : vars) {
    // Check and see if we've already constrained this variable. This is currently
    // only done when the bounds-safe interface has refined a type for an external
    // function, and we don't want the linking phase to un-refine it by introducing
    // a conflicting constraint.
    bool doAdd = true;
    if (checkSkip)
      if (ConstrainedVars.find(V) != ConstrainedVars.end())
        doAdd = false;

    if (doAdd)
      CS.addConstraint(CS.createEq(CS.getOrCreateVar(V), A));
  }

  if (FV)
    FV->constrainTo(CS, A, checkSkip);
}

bool PointerVariableConstraint::anyChanges(Constraints::EnvironmentMap &E) {
  bool f = false;

  for (const auto &C : vars) {
    VarAtom V(C);
    ConstAtom *CS = E[&V];
    assert(CS != nullptr);
    f |= isa<PtrAtom>(CS);
    f |= isa<NTArrAtom>(CS);
  }

  if (FV)
    f |= FV->anyChanges(E);

  return f;
}

bool PointerVariableConstraint::hasWild(Constraints::EnvironmentMap &E)
{
  for (const auto& C: vars) {
    VarAtom V(C);
    ConstAtom *CS = E[&V];
    assert(CS != nullptr);
    if (isa<WildAtom>(CS))
      return true;
  }

  if (FV)
    return FV->anyChanges(E);

  return false;
}

bool PointerVariableConstraint::hasArr(Constraints::EnvironmentMap &E)
{
  for (const auto& C: vars) {
    VarAtom V(C);
    ConstAtom *CS = E[&V];
    assert(CS != nullptr);
    if (isa<ArrAtom>(CS))
      return true;
  }

  if (FV)
    return FV->anyChanges(E);

  return false;
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
  bool addComma = false;
  for (const auto &I : returnVars) {
    if(addComma) {
      O << ",";
    }
    I->dump_json(O);
  }
  O << "], \"name\":\"" << name << "\", ";
  O << "\"Parameters\":[";
  addComma = false;
  for (const auto &I : paramVars) {
    if(I.size() > 0) {
      if (addComma) {
        O << ",\n";
      }
      O << "[";
      bool innerComma = false;
      for (const auto &J : I) {
        if(innerComma) {
          O << ",";
        }
        J->dump_json(O);
        innerComma = true;
      }
      O << "]";
      addComma = true;
    }
  }
  O << "]";
  O << "}}";
}

std::string
FunctionVariableConstraint::mkString(Constraints::EnvironmentMap &E, bool emitName, bool forItype) {
  std::string s = "";
  // TODO punting on what to do here. The right thing to do is to figure out
  // the LUB of all of the V in returnVars.
  assert(returnVars.size() > 0);
  ConstraintVariable *V = *returnVars.begin();
  assert(V != nullptr);
  s = V->mkString(E);
  s = s + "(";
  std::vector<std::string> parmStrs;
  for (const auto &I : this->paramVars) {
    // TODO likewise punting here.
    assert(I.size() > 0);
    ConstraintVariable *U = *(I.begin());
    assert(U != nullptr);
    parmStrs.push_back(U->mkString(E));
  }

  if (parmStrs.size() > 0) {
    std::ostringstream ss;

    std::copy(parmStrs.begin(), parmStrs.end() - 1,
              std::ostream_iterator<std::string>(ss, ", "));
    ss << parmStrs.back();

    s = s + ss.str() + ")";
  } else {
    s = s + ")";
  }

  return s;
}