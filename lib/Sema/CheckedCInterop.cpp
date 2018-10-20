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
/// a bounds expression Bounds.  Returns the empty type if no interop
/// type exists.
QualType Sema::SynthesizeInteropType(QualType Ty,
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

  // This code has been copied from TreeTransform.h and modified to first
  // update parameter/return types based on bounds annotations.
public:
  TransformFunctionTypeToChecked(Sema &SemaRef) : BaseTransform(SemaRef) {}

  bool IsInstantiation() { return false; }

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

    const FunctionProtoType *T = TL.getTypePtr();

  // Assert on trailing returning type for now, instead of handling them.
  // That's a C++ feature that we cannot test right now.
    if (T->hasTrailingReturn()) {
      assert(false && "Unexpected trailing return type for Checked C");
      return QualType();
    }

    // Update the parameters and return types to be their interop types. Also
    // update the extend prototype inofmration to remove interop type annotations.
    SmallVector<QualType, 4> CurrentParamTypes;
    SmallVector<BoundsAnnotations, 4> CurrentParamAnnots;
    TypeLoc ResultLoc = TL.getReturnLoc();
    FunctionProtoType::ExtProtoInfo EPI = T->getExtProtoInfo();
    bool EPIChanged = false;

    CurrentParamTypes.append(TL.getTypePtr()->param_type_begin(),
                             TL.getTypePtr()->param_type_end());

    // Update return type information.
    if (!EPI.ReturnAnnots.IsEmpty()) {
      if (ResultLoc.getType()->isUncheckedPointerType()) {
         InteropTypeExpr *IT = EPI.ReturnAnnots.getInteropTypeExpr();
         BoundsExpr *Bounds = EPI.ReturnAnnots.getBoundsExpr();
         assert(Bounds == nullptr || (Bounds != nullptr && IT));
         if (IT) {
           ResultLoc = IT->getTypeInfoAsWritten()->getTypeLoc();
#if TRACE_INTEROP
          llvm::outs() << "return bounds = ";
          if (Bounds)
            Bounds->dump(llvm::outs());
          else
            llvm::outs() << "none\n";
          llvm::outs() << "interop type = ";
          IT->dump(llvm::outs());
          llvm::outs() << "\nresult type = ";
          ResultLoc.getType().dump(llvm::outs());
#endif
          // Construct new annotations that do not have the bounds-safe interface type.
          if (Bounds) {
            EPI.ReturnAnnots = BoundsAnnotations(Bounds, nullptr);
          } else
            EPI.ReturnAnnots = BoundsAnnotations();
          EPIChanged = true;
        }
      }
    }

    // Update the parameter types and annotations. The EPI parameter array is still used
    // by the original type, so a create and update a copy in CurrentParamAnnots.
    if (EPI.ParamAnnots) {
      // Track whether there are parameter annotations left after removing interop
      // annotations.
      bool hasParamAnnots = false;
      for (unsigned int i = 0; i < CurrentParamTypes.size(); i++) {
        BoundsAnnotations IndividualAnnots =  EPI.ParamAnnots[i];
        CurrentParamAnnots.push_back(IndividualAnnots);

        if (CurrentParamTypes[i]->isUncheckedPointerType() &&
            IndividualAnnots.getInteropTypeExpr()) {
          InteropTypeExpr *IT = IndividualAnnots.getInteropTypeExpr();
          QualType ParamType = IT->getType();
          if (ParamType.isNull()) {
#if TRACE_INTEROP
            llvm::outs() << "encountered null parameter type with bounds";
            llvm::outs() << "\noriginal param type = ";
            CurrentParamTypes[i]->dump(llvm::outs());
            llvm::outs() << "\nparam interop type is:";
            IT->dump(llvm::outs());
            llvm::outs() << "\n";
            llvm::outs().flush();
#endif
            return QualType();
          }
          if (ParamType->isArrayType())
            ParamType = SemaRef.Context.getDecayedType(ParamType);
          CurrentParamTypes[i] = ParamType;
          // Remove the interop type annotation.
          BoundsExpr *Bounds = IndividualAnnots.getBoundsExpr();
          if (IT->getType()->isCheckedArrayType() && !Bounds)    
            Bounds = SemaRef.CreateCountForArrayType(IT->getType());
          if (Bounds) {
            hasParamAnnots = true;
            CurrentParamAnnots[i] = BoundsAnnotations(Bounds, nullptr);
          } else 
            CurrentParamAnnots[i] = BoundsAnnotations();
          EPIChanged = true;
        } else {
          if (!IndividualAnnots.IsEmpty())
            hasParamAnnots = true;
        }
      }

      if (hasParamAnnots)
        EPI.ParamAnnots = CurrentParamAnnots.data();
      else
        // If there are no parameter annotations left, null out the pointer to
        // the param annotations array.
        EPI.ParamAnnots = nullptr;
    }

    // Now transform the parameter/return types, parameter declarations, and
    // the EPI.

    // For the type location information, we must transform the return type
    // before pushing the function prototype type location record.

    // These hold the transformed results.
    SmallVector<QualType, 4> ParamTypes;
    SmallVector<ParmVarDecl*, 4> ParamDecls;
    Sema::ExtParameterInfoBuilder ExtParamInfos;
    // ParamAnnotsStorage is pre-allocated storage that is used when updating EPI
    // in TransformExtendedParameterInfo.  Its lifetime must last until the end of
    // the lifetime of EPI.
    SmallVector<BoundsAnnotations, 4> ParamAnnotsStorage;

    QualType ResultType = getDerived().TransformType(TLB, ResultLoc);
    if (ResultType.isNull())
      return QualType();

    // Places transformed data in ParamTypes, ParamDecls, and ExtParamInfos.
    if (getDerived().TransformFunctionTypeParams(
      TL.getBeginLoc(), TL.getParams(),
      CurrentParamTypes.begin(),
      T->getExtParameterInfosOrNull(),
      ParamTypes, &ParamDecls, ExtParamInfos))
      return QualType();

    if (getDerived().TransformExtendedParameterInfo(EPI, EPIChanged, ParamTypes,
                                                    ParamAnnotsStorage,
                                                    ExtParamInfos, TL,
                                                    TransformExceptionSpec))
      return QualType();

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
