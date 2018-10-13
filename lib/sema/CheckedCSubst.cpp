//===------ CheckedCInterop - Support Methods for Checked C Interop  -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements methods for doing type substitution and parameter
//  subsitution during semantic analysis.  This is used when typechecking
//  generic type application and checking bounds.
//
//===----------------------------------------------------------------------===//

#include "TreeTransform.h"

using namespace clang;
using namespace sema;

ExprResult Sema::ActOnTypeApplication(ExprResult TypeFunc, SourceLocation Loc,
  ArrayRef<DeclRefExpr::GenericInstInfo::TypeArgument> TypeArgs) {

  TypeFunc = CorrectDelayedTyposInExpr(TypeFunc);
  if (!TypeFunc.isUsable())
    return ExprError();

  // Make sure we have a generic function or function with a bounds-safe
  // interface.

  // TODO: generalize this
  DeclRefExpr *declRef = dyn_cast<DeclRefExpr>(TypeFunc.get());
  if (!declRef) {
    Diag(Loc, diag::err_type_args_limited);
    return ExprError();
  }

  const FunctionProtoType *funcType =
    declRef->getType()->getAs<FunctionProtoType>();

  if (!funcType) {
    Diag(Loc, diag::err_type_args_for_non_generic_expression);
    return ExprError();
  }

  // Make sure that the number of type names equals the number of type variables in 
  // the function type.
  if (funcType->getNumTypeVars() != TypeArgs.size()) {
    FunctionDecl* funDecl = dyn_cast<FunctionDecl>(declRef->getDecl());
    if (!funcType->isGenericFunction() && !funcType->isItypeGenericFunction()) {
      Diag(Loc,
       diag::err_type_args_for_non_generic_expression);
      // TODO: emit a note pointing to the declaration.
      return true;
    }

    // The location of beginning of _For_any is stored in typeVariables
    Diag(Loc,
      diag::err_type_list_and_type_variable_num_mismatch);

    if (funDecl)
      Diag(funDecl->typeVariables()[0]->getLocStart(),
        diag::note_type_variables_declared_at);

    return ExprError();
  }
  // Add parsed list of type names to declRefExpr for future references
  declRef->SetGenericInstInfo(Context, TypeArgs);

  // Substitute type arguments for type variables in the function type of the
  // DeclRefExpr.
  QualType NewTy = SubstituteTypeArgs(declRef->getType(), TypeArgs);
  declRef->setType(NewTy);
  return declRef;

}

// Recurse through types and substitute any TypedefType with TypeVariableType
// as the underlying type.
QualType Sema::SubstituteTypeArgs(QualType QT,
 ArrayRef<DeclRefExpr::GenericInstInfo::TypeArgument> TypeArgs) {
  const Type *T = QT.getTypePtr();
  switch (T->getTypeClass()) {
  case Type::Pointer: {
    const PointerType *pType = cast<PointerType>(T);
    QualType newPointee = SubstituteTypeArgs(pType->getPointeeType(), TypeArgs);
    return Context.getPointerType(newPointee, pType->getKind());
  }
  case Type::ConstantArray: {
    const ConstantArrayType *caType = cast<ConstantArrayType>(T);
    QualType EltTy = SubstituteTypeArgs(caType->getElementType(), TypeArgs);
    const llvm::APInt ArySize = caType->getSize();
    ArrayType::ArraySizeModifier ASM = caType->getSizeModifier();
    unsigned IndexTypeQuals = caType->getIndexTypeCVRQualifiers();
    CheckedArrayKind Kind = caType->getKind();
    return Context.getConstantArrayType(EltTy, ArySize, ASM, IndexTypeQuals, Kind);
  }
  case Type::VariableArray: {
    const VariableArrayType *vaType = cast<VariableArrayType>(T);
    QualType EltTy = SubstituteTypeArgs(vaType->getElementType(), TypeArgs);
    Expr *NumElts = vaType->getSizeExpr();
    ArrayType::ArraySizeModifier ASM = vaType->getSizeModifier();
    unsigned IndexTypeQuals = vaType->getIndexTypeCVRQualifiers();
    SourceRange Brackets = vaType->getBracketsRange();
    return Context.getVariableArrayType(EltTy, NumElts, ASM, IndexTypeQuals, Brackets);
  }
  case Type::IncompleteArray: {
    const IncompleteArrayType *iaType = cast<IncompleteArrayType>(T);
    QualType EltTy = SubstituteTypeArgs(iaType->getElementType(), TypeArgs);
    ArrayType::ArraySizeModifier ASM = iaType->getSizeModifier();
    unsigned IndexTypeQuals = iaType->getIndexTypeCVRQualifiers();
    CheckedArrayKind Kind = iaType->getKind();
    return Context.getIncompleteArrayType(EltTy, ASM, IndexTypeQuals, Kind);
  }
  case Type::DependentSizedArray: {
    const DependentSizedArrayType *dsaType = cast<DependentSizedArrayType>(T);
    QualType EltTy = SubstituteTypeArgs(dsaType->getElementType(), TypeArgs);
    Expr *NumElts = dsaType->getSizeExpr();
    ArrayType::ArraySizeModifier ASM = dsaType->getSizeModifier();
    unsigned IndexTypeQuals = dsaType->getIndexTypeCVRQualifiers();
    SourceRange Brackets = dsaType->getBracketsRange();
    return Context.getDependentSizedArrayType(EltTy, NumElts, ASM, IndexTypeQuals,
                                      Brackets);
  }
  case Type::Paren: {
    const ParenType *pType = dyn_cast<ParenType>(T);
    if (!pType) {
      llvm_unreachable("Dynamic cast failed unexpectedly.");
      return QT;
    }
    QualType InnerType = SubstituteTypeArgs(pType->getInnerType(), TypeArgs);
    return Context.getParenType(InnerType);
  }

  case Type::FunctionProto: {
    const FunctionProtoType *fpType = dyn_cast<FunctionProtoType>(T);
    if (!fpType) {
      llvm_unreachable("Dynamic cast failed unexpectedly.");
      return QT;
    }
    // Initialize return type if it's type variable
    BoundsAnnotations ReturnBAnnot = fpType->getReturnAnnots();
    InteropTypeExpr *RBIT = ReturnBAnnot.getInteropTypeExpr();
    QualType returnQualType;
    if (RBIT) {
      returnQualType = SubstituteTypeArgs(RBIT->getTypeAsWritten() , TypeArgs);
    } else {
      returnQualType = SubstituteTypeArgs(fpType->getReturnType() , TypeArgs);
    }
    // Initialize parameter types if it's type variable
    SmallVector<QualType, 16> newParamTypes;

    int iCnt = 0;
    for (QualType opt : fpType->getParamTypes()) {
      BoundsAnnotations BAnnot = fpType->getParamAnnots(iCnt);
      InteropTypeExpr *BIT = BAnnot.getInteropTypeExpr();
      if (BIT) {
        newParamTypes.push_back(SubstituteTypeArgs(BIT->getTypeAsWritten(), TypeArgs));
      } else {
        newParamTypes.push_back(SubstituteTypeArgs(opt, TypeArgs));
      }
      iCnt++;
    }
    // Indicate that this function type is fully instantiated
    FunctionProtoType::ExtProtoInfo newExtProtoInfo = fpType->getExtProtoInfo();
    newExtProtoInfo.numTypeVars = 0;
    newExtProtoInfo.GenericFunction = false;
    newExtProtoInfo.ItypeGenericFunction = false;

    // Recreate FunctionProtoType, and set it as the new type for declRefExpr
    return Context.getFunctionType(returnQualType, newParamTypes, newExtProtoInfo);
  }
    //bool changed = false;
    //const FunctionProtoType *fpType = cast<FunctionProtoType>(T);

    //// Make a copy of the ExtProtoInfo.
    //FunctionProtoType::ExtProtoInfo EPI = fpType->getExtProtoInfo();

    //// Handle return type and return bounds annotations.
    //QualType returnQualType = SubstituteTypeArgs(fpType->getReturnType() , TypeArgs);
    //if (returnQualType != fpType->getReturnType())
    //  changed = true;

    //BoundsAnnotations ReturnAnnots = EPI.ReturnAnnots;
    //InteropTypeExpr *RBIT = ReturnAnnots.getInteropTypeExpr();
    //// TODO: substitute into return bounds expression.
    //if (RBIT) {
    //  QualType ExistingType = RBIT->getTypeAsWritten();
    //  QualType NewType = substituteTypeArgs(ExistingType , TypeArgs);
    //  if (NewType != ExistingType) {
    //    TypeSourceInfo *TInfo =
    //      getTrivialTypeSourceInfo(NewType, SourceLocation());
    //    InteropTypeExpr *NewRBIT = 
    //       new (*this) InteropTypeExpr(NewType, RBIT->getLocStart(), RBIT->getLocEnd(), TInfo);
    //    EPI.ReturnAnnots = BoundsAnnotations(ReturnAnnots.getBoundsExpr(), NewRBIT);
    //    changed = true;
    //  }
    //}

    //// Handle parameter types and parameter bounds annotations.
    //// TODO: substitute into parameter bounds expressions.
    //SmallVector<QualType, 8> newParamTypes;
    //SmallVector<BoundsAnnotations, 8> newParamAnnots;

    //int i = 0;
    //for (QualType QT : fpType->getParamTypes()) {
    //  newParamTypes.push_back(substituteTypeArgs(QT, TypeArgs));
    //  BoundsAnnotations ParamAnnot = fpType->getParamAnnots(i);
    //  InteropTypeExpr *ParamInteropType = ParamAnnot.getInteropTypeExpr();
    //  InteropTypeExpr *NewParamInteropType = ParamInteropType;
    //  if (ParamInteropType) {
    //    QualType ExistingType = ParamInteropType->getTypeAsWritten();
    //    QualType NewType = substituteTypeArgs(ExistingType , TypeArgs);
    //    if (NewType != ExistingType) {
    //      TypeSourceInfo *TInfo =
    //        getTrivialTypeSourceInfo(NewType, SourceLocation());
    //      NewParamInteropType =       
    //         new (*this) InteropTypeExpr(NewType, ParamInteropType->getLocStart(), ParamInteropType->getLocEnd(), TInfo);
    //      changed = true;
    //    }
    //  }
    //  newParamAnnots.push_back(BoundsAnnotations(ParamAnnot.getBoundsExpr(), NewParamInteropType));
    //  i++;
    //}
    //// Indicate that this function type is fully instantiated

    //EPI.numTypeVars = 0;
    //EPI.ParamAnnots = newParamAnnots.data();  

    //// Recreate FunctionProtoType, and set it as the new type for declRefExpr
    //return getFunctionType(returnQualType, newParamTypes, EPI);
    //}
  case Type::Typedef: {
    // If underlying type is TypeVariableType, must replace the entire
    // TypedefType. If underlying type is not TypeVariableType, there cannot be
    // a type variable type that this context can replace.
    const TypedefType *tdType = cast<TypedefType>(T);
    const Type *underlying = tdType->getDecl()->getUnderlyingType().getTypePtr();
    if (const TypeVariableType *tvType = dyn_cast<TypeVariableType>(underlying)) 
      return TypeArgs[tvType->GetIndex()].typeName;
    else
      return QT;
  }
  case Type::TypeVariable: {
    // Although Type Variable Type is wrapped with Typedef Type, there may be
    // transformations in clang that eliminates Typedef.
    const TypeVariableType *tvType = cast<TypeVariableType>(T);
    return TypeArgs[tvType->GetIndex()].typeName;
  }
  case Type::Decayed: {
    const DecayedType *dType = cast<DecayedType>(T);
    QualType decayedType = SubstituteTypeArgs(dType->getOriginalType(), TypeArgs);
    return Context.getDecayedType(decayedType);
  }
  case Type::Adjusted: {
    const AdjustedType *aType = cast<AdjustedType>(T);
    QualType origType = SubstituteTypeArgs(aType->getOriginalType(), TypeArgs);
    QualType newType = SubstituteTypeArgs(aType->getAdjustedType(), TypeArgs);
    return Context.getAdjustedType(origType, newType);
  }
  default :
    return QT;
  }
}