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
      Diag(funDecl->typeVariables()[0]->getBeginLoc(),
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

namespace {
class TypeApplication : public TreeTransform<TypeApplication> {
  typedef TreeTransform<TypeApplication> BaseTransform;
  typedef ArrayRef<DeclRefExpr::GenericInstInfo::TypeArgument> TypeArgList;

private:
  TypeArgList TypeArgs;
  unsigned Depth;

public:
  TypeApplication(Sema &SemaRef, TypeArgList TypeArgs, unsigned Depth) :
    BaseTransform(SemaRef), TypeArgs(TypeArgs), Depth(Depth) {}

  QualType TransformTypeVariableType(TypeLocBuilder &TLB,
                                     TypeVariableTypeLoc TL) {
    const TypeVariableType *TV = TL.getTypePtr();
    unsigned TVDepth = TV->GetDepth();

    if (TVDepth < Depth) {
      // Case 1: the type variable is bound by a type quantifier (_Forall scope)
      // that lexically encloses the type quantifier that is being applied.
      // Nothing changes in this case.
      QualType Result = TL.getType();
      TypeVariableTypeLoc NewTL = TLB.push<TypeVariableTypeLoc>(Result);
      NewTL.setNameLoc(TL.getNameLoc());
      return Result;
    } else if (TVDepth == Depth) {
      // Case 2: the type variable is bound by the type quantifier that is
      // being applied.  Substitute the appropriate type argument.
      DeclRefExpr::GenericInstInfo::TypeArgument TypeArg = TypeArgs[TV->GetIndex()];
      TypeLoc NewTL =  TypeArg.sourceInfo->getTypeLoc();
      TLB.reserve(NewTL.getFullDataSize());
      // Run the type transform with the type argument's location information
      // so that the type type location class push on to the TypeBuilder is
      // the matching class for the transformed type.
      QualType Result = getDerived().TransformType(TLB, NewTL);
      // We don't expect the type argument to change.
      assert(Result == TypeArg.typeName);
      return Result;
    } else {
      // Case 3: the type variable is bound by a type quantifier nested within the
      // the one that is being applied.  Create a type variable with a decremented
      // depth, to account for the removal of the enclosing scope.
      QualType Result =
         SemaRef.Context.getTypeVariableType(TVDepth - 1, TV->GetIndex(),
                                             TV->IsBoundsInterfaceType());
      TypeVariableTypeLoc NewTL = TLB.push<TypeVariableTypeLoc>(Result);
      NewTL.setNameLoc(TL.getNameLoc());
      return Result;
    }
  }

  QualType TransformTypedefType(TypeLocBuilder &TLB, TypedefTypeLoc TL) {
    // Preserve typedef information, unless the underlying type has a type
    // variable embedded in it.
    const TypedefType *T = TL.getTypePtr();
    // See if the underlying type changes.
    QualType UnderlyingType = T->desugar();
    QualType TransformedType = getDerived().TransformType(UnderlyingType);
    if (UnderlyingType == TransformedType) {
      QualType Result = TL.getType();
      // It didn't change, so just copy the original type location information.
      TypedefTypeLoc NewTL = TLB.push<TypedefTypeLoc>(Result);
      NewTL.setNameLoc(TL.getNameLoc());
      return Result;
    }
    // Something changed, so we need to delete the typedef type from the AST and
    // and use the underlying transformed type.

    // Synthesize some dummy type source information.
    TypeSourceInfo *DI = getSema().Context.getTrivialTypeSourceInfo(UnderlyingType,
                                                getDerived().getBaseLocation());
    // Use that to get dummy location information.
    TypeLoc NewTL = DI->getTypeLoc();
    TLB.reserve(NewTL.getFullDataSize());
    // Re-run the type transformation with the dummy location information so
    // that the type location class pushed on to the TypeBuilder is the matching
    // class for the underlying type.
    QualType Result = getDerived().TransformType(TLB, NewTL);
    if (Result != TransformedType) {
      llvm::outs() << "Dumping transformed type:\n";
      Result.dump(llvm::outs());
      llvm::outs() << "Dumping result:\n";
      Result.dump(llvm::outs());
    }
    return Result;
  }
};
}

QualType Sema::SubstituteTypeArgs(QualType QT,
 ArrayRef<DeclRefExpr::GenericInstInfo::TypeArgument> TypeArgs) {
   if (QT.isNull())
     return QT;

   // Transform the type and strip off the quantifier.
   TypeApplication TypeApp(*this, TypeArgs, 0);
   QualType TransformedQT = TypeApp.TransformType(QT);

   // Something went wrong in the transformation.
   if (TransformedQT.isNull())
     return QT;

   const FunctionProtoType *FPT = TransformedQT->getAs<FunctionProtoType>();
   FunctionProtoType::ExtProtoInfo EPI = FPT->getExtProtoInfo();
   EPI.GenericFunction = false;
   EPI.ItypeGenericFunction = false;
   EPI.NumTypeVars = 0;
   QualType Result =
     Context.getFunctionType(FPT->getReturnType(), FPT->getParamTypes(), EPI);
    return Result;
}
