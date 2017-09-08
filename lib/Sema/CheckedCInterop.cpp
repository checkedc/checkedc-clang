//===------ CheckedCInterop - Support Methods for Checked C Interop  -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements support methods used during semantic analysis
//  of language constructs involving Checked C interoperation.
//
//===----------------------------------------------------------------------===//

#include "TreeTransform.h"

using namespace clang;
using namespace sema;

/// Get the corresponding Checked C interop type for Ty, given a
/// a bounds expression Bounds.
///
/// This falls into two cases:
/// 1. The interface is a type annotation: return the type in the
///    annotation.
/// 2. The interface is a bounds expression.  This implies the checked
/// type should be an _Array_ptr type or checked Array type.  Construct
/// the appropriate type from the unchecked type for the declaration and
/// return it.

QualType Sema::GetCheckedCInteropType(QualType Ty,
                                      const BoundsExpr *Bounds,
                                      bool isParam) {
  QualType ResultType = QualType();
  if (Ty.isNull() || Bounds == nullptr)
    return ResultType;

  // Parameter types that are array types are adusted to be
  // pointer types.  We'll work with the original type instead.
  // For multi-dimensional array types, the nested array types need
  // to become checked array types too.
  if (isParam) {
    if (const DecayedType *Decayed = dyn_cast<DecayedType>(Ty)) {
      Ty = Decayed->getOriginalType();
    }
  }

  switch (Bounds->getKind()) {
    case BoundsExpr::Kind::InteropTypeAnnotation: {
      const InteropTypeBoundsAnnotation *Annot =
        dyn_cast<InteropTypeBoundsAnnotation>(Bounds);
      assert(Annot && "unexpected dyn_cast failure");
      if (Annot != nullptr)
        ResultType = Annot->getType();
      break;
    }
    case BoundsExpr::Kind::ByteCount:
    case BoundsExpr::Kind::ElementCount:
    case BoundsExpr::Kind::Range: {
      if (const PointerType  *PtrType = Ty->getAs<PointerType>()) {
        if (PtrType->isUnchecked()) {
          ResultType = Context.getPointerType(PtrType->getPointeeType(),
                                              CheckedPointerKind::Array);
          ResultType.setLocalFastQualifiers(Ty.getCVRQualifiers());
        }
      }
      else if (Ty->isConstantArrayType() || Ty->isIncompleteArrayType())
        ResultType = MakeCheckedArrayType(Ty);
      break;
    }
    default:
      break;
  }

  // When a parameter variable declaration is created, array types for parameter
  // variables are adjusted to be pointer types.  We have to do the same here.
  if (isParam && !ResultType.isNull() && ResultType->isArrayType())
    ResultType = Context.getAdjustedParameterType(ResultType);

  return ResultType;
}

class RewriteBoundsSafeInterfaces : TreeTransform<RewriteBoundsSafeInterfaces> {
};

// Helper for rewriting function types with bounds-safe interfaces
// on parameters/returns to use checked types.
QualType Sema::RewriteBoundsSafeInterfaceTypes(QualType Ty, bool &Modified) {
  if (const PointerType *PtrType = Ty->getAs<PointerType>()) {
    QualType pointeeType =
      RewriteBoundsSafeInterfaceTypes(PtrType->getPointeeType(), Modified);
    if (!Modified)
      return Ty;
    QualType ResultType =
      Context.getPointerType(pointeeType, PtrType->getKind());
    ResultType.setLocalFastQualifiers(Ty.getCVRQualifiers());
    return ResultType;
  }
  else if (Ty->isArrayType()) {
    ASTContext &Context = this->Context;
    const ArrayType *arrTy = cast<ArrayType>(Ty.getCanonicalType().getTypePtr());
    QualType elemTy =
      RewriteBoundsSafeInterfaceTypes(arrTy->getElementType(), Modified);
    bool isChecked = arrTy->isChecked();
    if (!Modified)
      return Ty;

    switch (arrTy->getTypeClass()) {
      case Type::ConstantArray: {
        const ConstantArrayType *constArrTy = cast<ConstantArrayType>(arrTy);
        return Context.getConstantArrayType(elemTy,
                                            constArrTy->getSize(),
                                            constArrTy->getSizeModifier(),
                                            Ty.getCVRQualifiers(),
                                            isChecked);
      }
      case Type::IncompleteArray: {
        const IncompleteArrayType *incArrTy = cast<IncompleteArrayType>(arrTy);
        return Context.getIncompleteArrayType(elemTy,
                                              incArrTy->getSizeModifier(),
                                              Ty.getCVRQualifiers(),
                                              isChecked);
      }
      default:
        return Ty;
    }
  }
  else if (Ty->isFunctionProtoType()) {
  /*/
    const FunctionProtoType *FPT = dyn_cast<FunctionProtoType>(Ty);
    bool IsModified = false,
      bool ReturnTypeModified = false;
    QualType ReturnType = RewriteBoundsSafeInterfaceTypes(FPT->getReturnType(),
                                                          ReturnTypeModified);
    unsigned NumParams = FPT->getNumParams();
    IsModified |= ReturnTypeModified;


    )
    QualType Result = BuildFunctionType()
      // TODO: fill this in.
  */
    return Ty;
  }

  return Ty;
}

QualType Sema::RewriteBoundsSafeInterfaceTypes(QualType Ty) {
  bool Modified = false;
  return RewriteBoundsSafeInterfaceTypes(Ty, Modified);
}