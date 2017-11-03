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
  // Nothing to do.
  if (Ty.isNull() || Ty->isCheckedArrayType() ||
      Ty->isCheckedPointerType())
    return Ty;

  if (Bounds == nullptr)
    return QualType();

  if (Bounds->isUnknown())
    return Ty;

  QualType ResultType = QualType();

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
    case BoundsExpr::Kind::Invalid:
      break;
    case BoundsExpr::Kind::Unknown:
      llvm_unreachable("should already have been handled");
      break;
    case BoundsExpr::Kind::InteropTypeAnnotation: {
      const InteropTypeBoundsAnnotation *Annot =
        dyn_cast<InteropTypeBoundsAnnotation>(Bounds);
      assert(Annot && "unexpected dyn_cast failure");
      if (Annot != nullptr)
        ResultType = Annot->getType();
      break;
    }
    case BoundsExpr::Kind::Any:
    case BoundsExpr::Kind::ByteCount:
    case BoundsExpr::Kind::ElementCount:
    case BoundsExpr::Kind::Range: {
      if (const PointerType  *PtrType = Ty->getAs<PointerType>()) {
        if (PtrType->isUnchecked()) {
          ResultType = Context.getPointerType(PtrType->getPointeeType(),
                                              CheckedPointerKind::Array);
          ResultType.setLocalFastQualifiers(Ty.getCVRQualifiers());
        }
        else {
          assert(PtrType->isChecked());
          ResultType = Ty;
        }
      }
      else if (Ty->isConstantArrayType() || Ty->isIncompleteArrayType())
        ResultType = MakeCheckedArrayType(Ty);
      else
        llvm_unreachable("unexpected type with bounds annotation");
      break;
    }
  }

  // When a parameter variable declaration is created, array types for parameter
  // variables are adjusted to be pointer types.  We have to do the same here.
  if (isParam && !ResultType.isNull() && ResultType->isArrayType())
    ResultType = Context.getAdjustedParameterType(ResultType);

  return ResultType;
}

// Transform function types that have parameters or returns with bounds
// or bounds-safe interface types to ones that have checked types for
// those parameters or returns. The checked types are determined based
// on the bounds information combined with the type.
//
// This is tricky to do because function types may have function types
// embedded within them, and bounds or bounds-safe interfaces may also
// have function types embedded within them.  We rely on the AST TreeTransform
// functionality to do a thorough rewrite.

namespace {
class TransformFunctionTypeToChecked :
  public TreeTransform<TransformFunctionTypeToChecked> {

  typedef TreeTransform<TransformFunctionTypeToChecked> BaseTransform;

  // This method has been copied from TreeTransform.h and modified to add
  // an additional rewrite step that updates parameter/return types based
  // on bounds information.
  //
  // This code has been specialized to assert on trailing returning types
  // instead of handling them. That's a C++ feature that we could not test
  // for now.  The code could be added back later.

public:
  TransformFunctionTypeToChecked(Sema &SemaRef) : BaseTransform(SemaRef) {}

  QualType TransformFunctionProtoType(TypeLocBuilder &TLB,
                                      FunctionProtoTypeLoc TL) {
    SmallVector<QualType, 4> ExceptionStorage;
    TreeTransform *This = this; // Work around gcc.gnu.org/PR56135.
    return getDerived().TransformFunctionProtoType(
        TLB, TL, nullptr, 0,
        [&](FunctionProtoType::ExceptionSpecInfo &ESI, bool &Changed) {
          return This->TransformExceptionSpec(TL.getBeginLoc(), ESI,
                                              ExceptionStorage, Changed);
        });
  }
  template<typename Fn>
  QualType TransformFunctionProtoType(
    TypeLocBuilder &TLB, FunctionProtoTypeLoc TL, CXXRecordDecl *ThisContext,
    unsigned ThisTypeQuals, Fn TransformExceptionSpec) {

    // First rewrite any subcomponents so that nested function types are
    // handled.

    // Transform the parameters and return type.
    //
    // We are required to instantiate the params and return type in source order.
    //
    SmallVector<QualType, 4> ParamTypes;
    SmallVector<ParmVarDecl*, 4> ParamDecls;
    SmallVector<BoundsExpr *, 4> ParamBounds;
    Sema::ExtParameterInfoBuilder ExtParamInfos;
    const FunctionProtoType *T = TL.getTypePtr();

    QualType ResultType;
    if (T->hasTrailingReturn()) {
      assert("Unexpected trailing return type for Checked C");
      return QualType();
    }

    ResultType = getDerived().TransformType(TLB, TL.getReturnLoc());
    if (ResultType.isNull())
      return QualType();

    if (getDerived().TransformFunctionTypeParams(
      TL.getBeginLoc(), TL.getParams(),
      TL.getTypePtr()->param_type_begin(),
      T->getExtParameterInfosOrNull(),
      ParamTypes, &ParamDecls, ExtParamInfos))
      return QualType();

    FunctionProtoType::ExtProtoInfo EPI = T->getExtProtoInfo();
    bool EPIChanged = false;
    if (getDerived().TransformExtendedParameterInfo(EPI, ParamTypes, ParamBounds,
                                                    ExtParamInfos, TL,
                                                    TransformExceptionSpec,
                                                    EPIChanged))
      return QualType();

    // Now rewrite types based on bounds information.  Remove any
    // interop type annotations from bounds information also.

    if (const BoundsExpr *Bounds = EPI.ReturnBounds) {
      ResultType = SemaRef.GetCheckedCInteropType(ResultType, Bounds, false);
#if TRACE_INTEROP
      llvm::outs() << "return bounds = ";
      EPI.ReturnBounds->dump(llvm::outs());
      llvm::outs() << "\nreturn type = ";
      ResultType.dump(llvm::outs());
      llvm::outs() << "\nresult type = ";
      ResultType.dump(llvm::outs());
#endif
      // The types are structurally identical except for the checked bit,
      // so the type location information can still be used.
      TLB.TypeWasModifiedSafely(ResultType);

      // A return that has checked type should not have an interop type
      // annotation. If there is one, remove the annotation.  Array
      // types imply bounds annotations, but they can't appear as
      // the return type.
      if (Bounds->isInteropTypeAnnotation()) {
        assert(!Bounds->getType()->isCheckedArrayType());
        EPI.ReturnBounds = nullptr;
        EPIChanged = true;
      }
    }

    if (EPI.ParamBounds) {
      // Track whether there are parameter bounds left after removing interop
      // annotations.
      bool hasParamBounds = false;
      for (unsigned int i = 0; i < ParamTypes.size(); i++) {
        const BoundsExpr *IndividualBounds = ParamBounds[i];
        if (IndividualBounds) {
          QualType ParamType =
            SemaRef.GetCheckedCInteropType(ParamTypes[i], IndividualBounds,
                                           true);
          if (ParamType.isNull()) {
#if TRACE_INTEROP
            llvm::outs() << "encountered null parameter type with bounds";
            llvm::outs() << "\noriginal param type = ";
            ParamTypes[i]->dump(llvm::outs());
            llvm::outs() << "\nparam bounds are:";
            IndividualBounds->dump(llvm::outs());
            llvm::outs() << "\n";
            llvm::outs().flush();
#endif
            return QualType();
          }

          ParamTypes[i] = ParamType;
          // A parameter that has checked type should not have an interop type
          // annotation.  We need to remove the interop type annotation if there
          // is one.  There are two cases:
          // - the interop annotation is an array type: use the array type to
          // determine the new bounds for the parameter.
          // - the interop type annotation is not an array type.  Remove the
          // bounds for the parameter in the vector of new parameter bounds.
          if (IndividualBounds->isInteropTypeAnnotation()) {
            if (IndividualBounds->getType()->isCheckedArrayType()) {
              ParamBounds[i] =
                SemaRef.CreateCountForArrayType(IndividualBounds->getType());
              EPIChanged = true;
              hasParamBounds = true;
            } else {
              ParamBounds[i] = nullptr;
              EPIChanged = true;
            }
          } else
            hasParamBounds = true;
        }
      }

      // If there are no parameter bounds left, null out the pointer to the
      // param bounds array.
      if (!hasParamBounds)
        EPI.ParamBounds = nullptr;
    }

    // Rebuild the type if something changed.
    QualType Result = TL.getType();
    if (getDerived().AlwaysRebuild() || ResultType != T->getReturnType() ||
        T->getParamTypes() != llvm::makeArrayRef(ParamTypes) || EPIChanged) {
      Result = getDerived().RebuildFunctionProtoType(ResultType, ParamTypes, EPI);
      if (Result.isNull()) {
#if TRACE_INTEROP
        llvm::outs() << "Rebuild function prototype failed";
#endif
        return QualType();
      }
    }

    FunctionProtoTypeLoc NewTL = TLB.push<FunctionProtoTypeLoc>(Result);
    NewTL.setLocalRangeBegin(TL.getLocalRangeBegin());
    NewTL.setLParenLoc(TL.getLParenLoc());
    NewTL.setRParenLoc(TL.getRParenLoc());
    NewTL.setExceptionSpecRange(TL.getExceptionSpecRange());
    NewTL.setLocalRangeEnd(TL.getLocalRangeEnd());
    for (unsigned i = 0, e = NewTL.getNumParams(); i != e; ++i)
      NewTL.setParam(i, ParamDecls[i]);
#if TRACE_INTEROP
    llvm::outs() << "Result function type = ";
    Result.dump(llvm::outs());
    llvm::outs() << "\n";
#endif
    return Result;
  }

  QualType TransformTypedefType(TypeLocBuilder &TLB, TypedefTypeLoc TL) {
    // Preserve typedef information, unless the underlying type has a function type
    // embedded in it with a bounds-safe interface.
    const TypedefType *T = TL.getTypePtr();
    // See if the underlying type changes.
    QualType UnderlyingType = T->desugar();
    QualType TransformedType = TransformType(UnderlyingType);
    if (UnderlyingType == TransformedType) {
      QualType Result = TL.getType();
      // It didn't change, so just copy the original type location information.
      TypedefTypeLoc NewTL = TLB.push<TypedefTypeLoc>(Result);
      NewTL.setNameLoc(TL.getNameLoc());
      return Result;
    }
    // Something changed, so we need to delete the typedef from the AST and
    // and use the underlying transformed type.

    // Synthesize some dummy type source information.
    TypeSourceInfo *DI = getSema().Context.getTrivialTypeSourceInfo(TransformedType,
                                                getDerived().getBaseLocation());
    // Use that to get dummy location information.
    TypeLoc NewTL = DI->getTypeLoc();
    TLB.reserve(NewTL.getFullDataSize());
    // Re-run the type transformation with the dummy location information so
    // that the type location class pushed on to the TypeBuilder is the matching
    // class for the underlying type.
    QualType Result = getDerived().TransformType(TLB, NewTL);
    assert(Result == TransformedType);
    return Result;
  }
};
}

QualType Sema::RewriteBoundsSafeInterfaceTypes(QualType Ty) {
  return TransformFunctionTypeToChecked(*this).TransformType(Ty);
}
