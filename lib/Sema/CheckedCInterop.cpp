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

/// \brief Create the corresponding Checked C interop type for Ty, given a
/// a bounds expression Bounds.
///
/// The checked type should be an _Array_ptr type or checked Array type.  
/// Constructthe appropriate type from the unchecked type for the declaration
/// and return it.
QualType Sema::CreateCheckedCInteropType(QualType Ty,
                                         bool isParam) {
  // Nothing to do.
  if (Ty.isNull() || Ty->isCheckedArrayType() ||
    Ty->isCheckedPointerType())
    return QualType();

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

  // When a parameter variable declaration is created, array types for parameter
  // variables are adjusted to be pointer types.  We have to do the same here.
  if (isParam && !ResultType.isNull() && ResultType->isArrayType())
    ResultType = Context.getAdjustedParameterType(ResultType);

  return ResultType;
}

/// \brief Return the interop type for BA.  Adjust it if necessary for
/// parameters, where the interop type could be an array type.  If BA
/// is nul, return an empty qualified type.
QualType Sema::AdjustInteropType(const InteropTypeExpr *BA, bool IsParam) {
  if (!BA) return QualType();
  QualType ResultType = BA->getType();
  if (IsParam && !ResultType.isNull() && ResultType->isArrayType())
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
    SmallVector<BoundsAnnotations *, 4> ParamAnnots;
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
    if (getDerived().TransformExtendedParameterInfo(EPI, ParamTypes, ParamAnnots,
                                                    ExtParamInfos, TL,
                                                    TransformExceptionSpec,
                                                    EPIChanged))
      return QualType();

    // Now rewrite types based on interop type information, and remove
    // the interop types.
    if (const BoundsAnnotations *Annots = EPI.ReturnAnnots) {
      if (ResultType->isUncheckedPointerType()) {
         InteropTypeExpr *IT = Annots->getInteropTypeExpr();
         BoundsExpr *Bounds = Annots->getBoundsExpr();
         assert(Bounds == nullptr || (Bounds != nullptr && IT));
         if (IT) {
           ResultType = IT->getType();
#if TRACE_INTEROP
          llvm::outs() << "return bounds = ";
          if (Bounds)
            Bounds->dump(llvm::outs());
          else
            llvm::outs() << "none\n";
          llvm::outs() << "interop type = ";
          IT->dump(llvm::outs());
          llvm::outs() << "\nresult type = ";
          ResultType.dump(llvm::outs());
#endif
          // The types are structurally identical except for the checked bit,
          // so the type location information can still be used.
          TLB.TypeWasModifiedSafely(ResultType);

          // Construct new annotations that do not have the bounds-safe interface type.
          if (Bounds) {
            BoundsAnnotations *NewBA = new (SemaRef.Context) BoundsAnnotations(Bounds, nullptr);
            EPI.ReturnAnnots = NewBA;
          } else 
            EPI.ReturnAnnots = nullptr;
          EPIChanged = true;
        }
      }
    }

    if (EPI.ParamAnnots) {
      // Track whether there are parameter annotations left after removing interop
      // annotations.
      bool hasParamAnnots = false;
      for (unsigned int i = 0; i < ParamTypes.size(); i++) {
        BoundsAnnotations *IndividualAnnots = ParamAnnots[i];
        if (ParamTypes[i]->isUncheckedPointerType() && IndividualAnnots &&
            IndividualAnnots->getInteropTypeExpr()) {
          InteropTypeExpr *IT = IndividualAnnots->getInteropTypeExpr();
          QualType ParamType = IT->getType();
          if (ParamType.isNull()) {
#if TRACE_INTEROP
            llvm::outs() << "encountered null parameter type with bounds";
            llvm::outs() << "\noriginal param type = ";
            ParamTypes[i]->dump(llvm::outs());
            llvm::outs() << "\nparam interop type is:";
            IT->dump(llvm::outs());
            llvm::outs() << "\n";
            llvm::outs().flush();
#endif
            return QualType();
          }
          if (ParamType->isArrayType())
            ParamType = SemaRef.Context.getDecayedType(ParamType);
          ParamTypes[i] = ParamType;
          // Remove the interop type annotation.
          BoundsExpr *Bounds = IndividualAnnots->getBoundsExpr();
          if (IT->getType()->isCheckedArrayType()) {
            assert(!Bounds);              
            Bounds = SemaRef.CreateCountForArrayType(IT->getType());
          }
          if (Bounds) {
            hasParamAnnots = true;
            BoundsAnnotations *NewBA = new (getSema().Context) BoundsAnnotations(Bounds, nullptr);
            ParamAnnots[i] = NewBA;
          } else 
            ParamAnnots[i] = nullptr;
          EPIChanged = true;
        } else {
          ParamAnnots[i] = IndividualAnnots;
          if (IndividualAnnots)
            hasParamAnnots = true;
        }
      }

      // If there are no parameter bounds left, null out the pointer to the
      // param annotations array.
      if (!hasParamAnnots)
        EPI.ParamAnnots = nullptr;
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
