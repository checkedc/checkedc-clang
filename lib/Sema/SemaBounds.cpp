//===---------- SemaBounds.cpp - Operations On Bounds Expressions --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements operations on bounds expressions for semantic analysis.
//  The operations include:
//  * Abstracting bounds expressions so that they can be used in function types.
//    This also checks that requirements on variable references are met and
//    emit diagnostics if they are not.
//
//    The abstraction also removes extraneous details:
//    - References to ParamVarDecl's are abstracted to positional index numbers
//      in argument lists.
//    - References to other VarDecls's are changed to use canonical
//      declarations.
//
//    Line number information is left in place for expressions, though.  It
//    would be a lot of work to write functions to change the line numbers to
//    the invalid line number. The canonicalization of types ignores line number
//    information in determining if two expressions are the same.  Users of bounds
//    expressions that have been abstracted need to be aware that line number
//    information may be inaccurate.
//  * Concretizing bounds expressions from function types.  This undoes the
//    abstraction by substituting parameter varaibles for the positional index
//    numbers.
//===----------------------------------------------------------------------===//

#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/ADT/SmallBitVector.h"
#include "TreeTransform.h"

using namespace clang;
using namespace sema;

namespace {
  class AbstractBoundsExpr : public TreeTransform<AbstractBoundsExpr> {
    typedef TreeTransform<AbstractBoundsExpr> BaseTransform;
    typedef ArrayRef<DeclaratorChunk::ParamInfo> ParamsInfo;

  private:
    const ParamsInfo Params;

    // TODO: change this constant when we want to error on global variables
    // in parameter bounds declarations.
    const bool errorOnGlobals = false;

  public:
    AbstractBoundsExpr(Sema &SemaRef, ParamsInfo Params) :
      BaseTransform(SemaRef), Params(Params) {}

    Decl *TransformDecl(SourceLocation Loc, Decl *D) {
      return D->getCanonicalDecl();
    }

    ExprResult TransformDeclRefExpr(DeclRefExpr *E) {
      ValueDecl *D = E->getDecl();
      if (VarDecl *V = dyn_cast<VarDecl>(D)) {
        if (V->isLocalVarDecl())
          // Parameter bounds may not be in terms of local variables
          SemaRef.Diag(E->getLocation(),
                       diag::err_out_of_scope_function_type_local);
        else if (V->isFileVarDecl() || V->isExternC()) {
          // Parameter bounds may not be in terms of "global" variables
          // TODO: This is guarded by a flag right now, as we don't yet
          // want to error everywhere.
          if (errorOnGlobals) {
            SemaRef.Diag(E->getLocation(),
                          diag::err_out_of_scope_function_type_global);
          }
        }
        else if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
          // Parameter bounds may be in terms of other parameters,
          // in which case we'll convert to a position-based representation.
          for (auto &ParamInfo : Params)
            if (PD == ParamInfo.Param) {
              return SemaRef.CreatePositionalParameterExpr(
                PD->getFunctionScopeIndex(),
                PD->getType());
            }
          SemaRef.Diag(E->getLocation(),
                       diag::err_out_of_scope_function_type_parameter);
        }
      }

      ValueDecl *ND =
        dyn_cast_or_null<ValueDecl>(BaseTransform::TransformDecl(
          SourceLocation(), D));
      if (D == ND || ND == nullptr)
        return E;
      else {
        clang::NestedNameSpecifierLoc QualifierLoc  = E->getQualifierLoc();
        clang::DeclarationNameInfo NameInfo = E->getNameInfo();
        return getDerived().RebuildDeclRefExpr(QualifierLoc, ND, NameInfo,
                                                nullptr);
      }
    }
  };
}

BoundsExpr *Sema::AbstractForFunctionType(
  BoundsExpr *Expr,
  ArrayRef<DeclaratorChunk::ParamInfo> Params) {
  if (!Expr)
    return Expr;

  BoundsExpr *Result;
  ExprResult AbstractedBounds =
    AbstractBoundsExpr(*this, Params).TransformExpr(Expr);
  if (AbstractedBounds.isInvalid()) {
    llvm_unreachable("unexpected failure to abstract bounds");
    Result = nullptr;
  }
  else {
    Result = dyn_cast<BoundsExpr>(AbstractedBounds.get());
    assert(Result && "unexpected dyn_cast failure");
    return Result;
  }
}

namespace {
  class ConcretizeBoundsExpr : public TreeTransform<ConcretizeBoundsExpr> {
    typedef TreeTransform<ConcretizeBoundsExpr> BaseTransform;

  private:
    ArrayRef<ParmVarDecl *> Parameters;

  public:
    ConcretizeBoundsExpr(Sema &SemaRef, ArrayRef<ParmVarDecl *> Params) :
      BaseTransform(SemaRef),
      Parameters(Params) { }

    ExprResult TransformPositionalParameterExpr(PositionalParameterExpr *E) {
      unsigned index = E->getIndex();
      if (index < Parameters.size()) {
        ParmVarDecl *PD = Parameters[index];
        return SemaRef.BuildDeclRefExpr(PD, E->getType(),
          clang::ExprValueKind::VK_LValue, SourceLocation());
      } else {
        llvm_unreachable("out of range index for positional parameter");
        return ExprResult();
      }
    }
  };
}

BoundsExpr *Sema::ConcretizeFromFunctionType(BoundsExpr *Expr,
                                             ArrayRef<ParmVarDecl *> Params) {
  if (!Expr)
    return Expr;

  BoundsExpr *Result;
  ExprResult ConcreteBounds = ConcretizeBoundsExpr(*this, Params).TransformExpr(Expr);
  if (ConcreteBounds.isInvalid()) {
    llvm_unreachable("unexpected failure in making bounds concrete");
    return nullptr;
  }
  else {
    Result = dyn_cast<BoundsExpr>(ConcreteBounds.get());
    assert(Result && "unexpected dyn_cast failure");
    return Result;
  }
}

namespace {
  class ConcretizeBoundsExprWithArgs : public TreeTransform<ConcretizeBoundsExprWithArgs> {
    typedef TreeTransform<ConcretizeBoundsExprWithArgs> BaseTransform;

  private:
    const ArrayRef<Expr *> Arguments;
    // This stores whether we've emitted an error for a particular substitution
    // so that we don't duplicate error messages.
    llvm::SmallBitVector ErroredForArgument;
    Sema::NonModifyingContext ErrorKind;
    bool SubstitutedModifyingExpression;


  public:
    ConcretizeBoundsExprWithArgs(Sema &SemaRef, ArrayRef<Expr *> Args,
                                 Sema::NonModifyingContext ErrorKind) :
      BaseTransform(SemaRef),
      Arguments(Args),
      ErroredForArgument(Args.size()),
      ErrorKind(ErrorKind),
      SubstitutedModifyingExpression(false) { }

    bool substitutedModifyingExpression() {
      return SubstitutedModifyingExpression;
    }

    ExprResult TransformPositionalParameterExpr(PositionalParameterExpr *E) {
      unsigned index = E->getIndex();
      if (index < Arguments.size()) {
        Expr *AE = Arguments[index];
        bool ShouldReportError = !ErroredForArgument[index];

        // We may only substitute if this argument expression is
        // a non-modifying expression.
        if (!SemaRef.CheckIsNonModifying(AE, ErrorKind,
                                         ShouldReportError)) {
          SubstitutedModifyingExpression = true;
          ErroredForArgument.set(index);
        }

        return AE;
      } else {
        llvm_unreachable("out of range index for positional argument");
        return ExprResult();
      }
    }
  };
}

BoundsExpr *Sema::ConcretizeFromFunctionTypeWithArgs(
  BoundsExpr *Bounds, ArrayRef<Expr *> Args,
  NonModifyingContext ErrorKind) {
  if (!Bounds)
    return Bounds;

  BoundsExpr *Result;
  auto Concretizer = ConcretizeBoundsExprWithArgs(*this, Args, ErrorKind);
  ExprResult ConcreteBounds = Concretizer.TransformExpr(Bounds);
  if (Concretizer.substitutedModifyingExpression()) {
      return nullptr;
  }
  else if (ConcreteBounds.isInvalid()) {
    llvm_unreachable("unexpected failure in making function bounds concrete with arguments");
    return nullptr;
  }
  else {
    Result = dyn_cast<BoundsExpr>(ConcreteBounds.get());
    assert(Result && "unexpected dyn_cast failure");
    return Result;
  }
}

namespace {
  class ConcretizeMemberBounds : public TreeTransform<ConcretizeMemberBounds> {
    typedef TreeTransform<ConcretizeMemberBounds> BaseTransform;

  private:
    Expr *Base;
    bool IsArrow;

  public:
    ConcretizeMemberBounds(Sema &SemaRef, Expr *MemberBaseExpr, bool IsArrow) :
      BaseTransform(SemaRef), Base(MemberBaseExpr), IsArrow(IsArrow) { }

    // TODO: handle the situation where the base expression is an rvalue.
    // By C semantics, the result is an rvalue.  We are setting fields used in
    // bounds expressions to be lvalues, so we end up with a problems when
    // we expand the occurrences of the fields to be expressions that are
    //  rvalues.
    //
    // There are two problematic cases:
    // - We assume field expressions are lvalues, so we will have lvalue-to-rvalue
    //   conversions applied to rvalues.  We need to remove these conversions.
    // - The address of a field is taken.  It is illegal to take the address of
    //   an lvalue.
    //
    // rVvalue structs can arise from function returns of struct values.
    ExprResult TransformDeclRefExpr(DeclRefExpr *E) {
      if (FieldDecl *FD = dyn_cast<FieldDecl>(E->getDecl())) {
        if (Base->isRValue() && !IsArrow)
          // For now, return nothing if we see an rvalue base.
          return ExprResult();
        ASTContext &Context = SemaRef.getASTContext();
        ExprValueKind ResultKind;
        if (IsArrow)
          ResultKind = VK_LValue;
        else
          ResultKind = Base->isLValue() ? VK_LValue : VK_RValue;
        MemberExpr *ME =
          new (Context) MemberExpr(Base, IsArrow,
                                   SourceLocation(), FD, SourceLocation(),
                                   E->getType(), ResultKind, OK_Ordinary);
        return ME;
      }
      return E;
    }
  };
}


BoundsExpr *Sema::MakeMemberBoundsConcrete(
  Expr *Base,
  bool IsArrow,
  BoundsExpr *Bounds) {
  ExprResult ConcreteBounds =
    ConcretizeMemberBounds(*this, Base, IsArrow).TransformExpr(Bounds);
  if (ConcreteBounds.isInvalid())
    return nullptr;
  else {
    BoundsExpr *Result = dyn_cast<BoundsExpr>(ConcreteBounds.get());
    return Result;
  }
}

namespace {
// Compute what positional parameters are used by an expression.
class CollectPositionalParameters : public RecursiveASTVisitor<CollectPositionalParameters> {

private:
  llvm::SmallBitVector Used;

public:
  CollectPositionalParameters(unsigned ParamCount) : Used(ParamCount) {}

  bool VisitPositionalParameterExpr(PositionalParameterExpr *PE) {
    Used.set(PE->getIndex());
    return true;
  }

  bool IsUsed(unsigned Index) {
    return Used.test(Index);
  }
};
}

namespace {
  // Class for inferring bounds expressions for C expressions.

  // C has an interesting semantics for expressions that differentiates between
  // lvalue and value expressions and inserts implicit conversions from lvalues
  // to values.  Value expressions are usually called rvalue expressions.  This
  // semantics is represented directly in the clang IR by having some
  // expressions evaluate to lvalues and having implicit conversions that convert
  // those lvalues to rvalues.
  //
  // Using ths representation directly would make it clumsy to compute bounds
  // expressions.  For an expression that evaluates to an lvalue, we would have
  // to compute and carry along two bounds expressions: the bounds expression
  // for the lvalue and the bounds expression for the value at which the lvalue
  // points.
  //
  // We address this by having three methods for computing bounds.  One method
  // (RValueBounds) computes the bounds for an rvalue expression. For lvalue
  // expressions, we have two methods that compute the bounds.  LValueBounds
  // computes the bounds for the lvalue produced by an expression.
  // LValueTargetBounds computes the bounds for the target of the lvalue
  // produced by the expression.  The method to use depends on the context in
  // which the lvalue expression is used.
  //
  // There are only a few contexts where an lvalue expression can occur, so it
  // is straightforward to determine which method to use. Also, the clang IR
  // makes it explicit when an lvalue is converted to an rvalue by an lvalue
  // cast operation.
  //
  // An expression denotes an lvalue if it occurs in the following contexts:
  // 1. As the left-hand side of an assignment operator.
  // 2. As the operand to a postfix or prefix incrementation operators (which
  //    implicitly do assignment).
  // 3. As the operand of the address-of (&) operator.
  // 4. If a member access operation e1.f denotes on lvalue, e1 denotes an
  //    lvalue.
  // 5. In clang IR, as an operand to an LValueToRValue cast operation.
  // Otherwise an expression denotes an rvalue.
  class BoundsInference {

  private:
    // TODO: be more flexible about where bounds expression are allocated.
    Sema &SemaRef;
    ASTContext &Context;

    BoundsExpr *CreateBoundsUnknown() {
      return new (Context) NullaryBoundsExpr(BoundsExpr::Kind::Unknown,
                                             SourceLocation(),
                                             SourceLocation());
    }

    // This describes an empty range. We use this where semantically the value
    // can never point to any range of memory, and statically understanding this
    // is useful.
    // We use this for example for function pointers or float-typed expressions.
    //
    // This is better than represenging the empty range as bounds(e, e), or even
    // bounds(e1, e2), because in these cases we need to do further analysis to
    // understand that the upper and lower bounds of the range are equal.
    BoundsExpr *CreateBoundsEmpty() {
      return CreateBoundsUnknown();
    }

    // This describes that this is an expression we will never
    // be able to infer bounds for.
    BoundsExpr *CreateBoundsAlwaysUnknown() {
      return CreateBoundsUnknown();
    }

    // If we have an error in our bounds inference that we can't
    // recover from, bounds(unknown) is our error value
    BoundsExpr *CreateBoundsInferenceError() {
      return CreateBoundsUnknown();
    }

    // This describes the bounds of null, which is compatible with every
    // other bounds annotation.
    BoundsExpr *CreateBoundsAny() {
      return new (Context) NullaryBoundsExpr(BoundsExpr::Kind::Any,
                                             SourceLocation(),
                                             SourceLocation());
    }

    // Currently our inference algorithm has some limitations,
    // where we cannot express bounds for things that will have bounds
    //
    // This is for the case where we want to allow these today,
    // but we need to re-visit these places and disallow some instances
    // when we can accurately calculate these bounds.
    BoundsExpr *CreateBoundsAllowedButNotComputed() {
      return CreateBoundsAny();
    }
    // This is for the opposite case, where we want to return bounds(unknown)
    // at the moment, but we want to re-visit these parts of inference
    // and in some cases compute bounds.
    BoundsExpr *CreateBoundsNotAllowedYet() {
      return CreateBoundsUnknown();
    }

    BoundsExpr *CreateSingleElementBounds() {
      IntegerLiteral *One =
        CreateIntegerLiteral(llvm::APInt(1, 1, /*isSigned=*/false));
      BoundsExpr *Count = new (Context) CountBoundsExpr(BoundsExpr::Kind::ElementCount,
                                                        One, SourceLocation(),
                                                        SourceLocation());
      return Count;
    }

    BoundsExpr *CreateSingleElementBounds(Expr *LowerBounds) {
      assert(LowerBounds->isRValue());
      // Create an unsigned integer 1
      IntegerLiteral *One =
        CreateIntegerLiteral(llvm::APInt(1, 1, /*isSigned=*/false));
      CountBoundsExpr CBE = CountBoundsExpr(BoundsExpr::Kind::ElementCount,
                                            One, SourceLocation(),
                                            SourceLocation());
      return ExpandToRange(LowerBounds, &CBE);
    }

  public:
    ImplicitCastExpr *CreateImplicitCast(QualType Target, CastKind CK,
                                         Expr *E) {
      return ImplicitCastExpr::Create(Context, Target, CK, E, nullptr,
                                       ExprValueKind::VK_RValue);
    }

  private:
    Expr *CreateExplicitCast(QualType Target, CastKind CK, Expr *E) {
      return CStyleCastExpr::Create(Context, Target, ExprValueKind::VK_RValue,
                                      CK, E, nullptr, nullptr, SourceLocation(),
                                      SourceLocation());
    }

    Expr *CreateAddressOfOperator(Expr *E) {
      QualType Ty = Context.getPointerType(E->getType(), CheckedPointerKind::Array);
      return new (Context) UnaryOperator(E, UnaryOperatorKind::UO_AddrOf, Ty,
                                         ExprValueKind::VK_RValue,
                                         ExprObjectKind::OK_Ordinary,
                                         SourceLocation());
    }

    IntegerLiteral *CreateIntegerLiteral(const llvm::APInt &I) {
      uint64_t Bits = I.getZExtValue();
      unsigned Width = Context.getIntWidth(Context.UnsignedLongLongTy);
      llvm::APInt ResultVal(Width, Bits);
      IntegerLiteral *Lit = IntegerLiteral::Create(Context, ResultVal,
                                                   Context.UnsignedLongLongTy,
                                                   SourceLocation());
      return Lit;
    }

  public:
    // Given an array type with constant dimension size, produce a count
    // expression with that size.
    BoundsExpr *CreateBoundsForArrayType(QualType QT) {
      const ConstantArrayType *CAT = Context.getAsConstantArrayType(QT);
      if (!CAT)
        return CreateBoundsAlwaysUnknown();

      llvm::APInt size = CAT->getSize();
      // Null-terminated arrays of size n have bounds of count(n - 1).
      // The null terminator is excluded from the count.
      if (CAT->getKind() == CheckedArrayKind::NtChecked) {
        assert(size.uge(1) && "must have at least one element");
        size = size - 1;
      }
      IntegerLiteral *Size = CreateIntegerLiteral(size);
      CountBoundsExpr *CBE =
         new (Context) CountBoundsExpr(BoundsExpr::Kind::ElementCount,
                                       Size, SourceLocation(),
                                       SourceLocation());
      return CBE;
    }

    // Given a byte_count or count bounds expression for the expression Base,
    // expand it to a range bounds expression:
    //  E : Count(C) expands to Bounds(E, E + C)
    //  E : ByteCount(C)  exzpands to Bounds((char *) E, (char *) E + C)
    BoundsExpr *ExpandToRange(Expr *Base, BoundsExpr *B) {
      assert(Base->isRValue() && "expected rvalue expression");
      BoundsExpr::Kind K = B->getKind();
      switch (K) {
        case BoundsExpr::Kind::ByteCount:
        case BoundsExpr::Kind::ElementCount: {
          CountBoundsExpr *BC = dyn_cast<CountBoundsExpr>(B);
          if (!BC) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }
          Expr *Count = BC->getCountExpr();
          QualType ResultTy;
          Expr *LowerBound;
          if (K == BoundsExpr::ByteCount) {
            ResultTy = Context.getPointerType(Context.CharTy,
                                              CheckedPointerKind::Array);
            // When bounds are pretty-printed as source code, the cast needs
            // to appear in the source code for the code to be correct, so
            // use an explicit cast operation.
            LowerBound =
              CreateExplicitCast(ResultTy, CastKind::CK_BitCast, Base);
          } else {
            ResultTy = Base->getType();
            LowerBound = Base;
          }
          Expr *UpperBound =
            new (Context) BinaryOperator(LowerBound, Count,
                                          BinaryOperatorKind::BO_Add,
                                          ResultTy,
                                          ExprValueKind::VK_RValue,
                                          ExprObjectKind::OK_Ordinary,
                                          SourceLocation(),
                                          FPOptions());
          return new (Context) RangeBoundsExpr(LowerBound, UpperBound,
                                               SourceLocation(),
                                               SourceLocation());
        }
        case BoundsExpr::Kind::InteropTypeAnnotation:
          return CreateBoundsUnknown();
        default:
          return B;
      }
    }

  public:
    BoundsInference(Sema &S) : SemaRef(S), Context(S.getASTContext()) {
    }

    // Compute bounds for a variable expression or member reference expression
    // with an array type.
    BoundsExpr *ArrayExprBounds(Expr *E) {
      DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
      assert((DR && dyn_cast<VarDecl>(DR->getDecl())) || isa<MemberExpr>(E));
      BoundsExpr *BE = CreateBoundsForArrayType(E->getType());
      if (BE->isUnknown())
        return BE;

      Expr *Base = CreateImplicitCast(Context.getDecayedType(E->getType()),
                                      CastKind::CK_ArrayToPointerDecay,
                                      E);
      return ExpandToRange(Base, BE);
    }


    // Infer bounds for an lvalue.  The bounds determine whether
    // it is valid to access memory using the lvalue.  The bounds
    // should be the range of an object in memory or a subrange of
    // an object.
    BoundsExpr *LValueBounds(Expr *E) {
      // E may not be an lvalue if there is a typechecking error when struct 
      // accesses member array incorrectly.
      if (!E->isLValue()) return CreateBoundsInferenceError();
      // TODO: handle side effects within E
      E = E->IgnoreParens();
      switch (E->getStmtClass()) {
      case Expr::DeclRefExprClass: {
        DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
        if (!DR) {
          llvm_unreachable("unexpected cast failure");
          return CreateBoundsInferenceError();
        }

        if (DR->getType()->isArrayType()) {
          VarDecl *VD = dyn_cast<VarDecl>(DR->getDecl());
          if (!VD) {
            llvm_unreachable("declref with array type not a vardecl");
            return CreateBoundsInferenceError();
          }
          // Declared bounds override the bounds based on the array type.
          BoundsExpr *B = VD->getBoundsExpr();
          if (B && !B->isInteropTypeAnnotation()) {
            Expr *Base = CreateImplicitCast(Context.getDecayedType(E->getType()),
                                            CastKind::CK_ArrayToPointerDecay,
                                            E);
            return ExpandToRange(Base, B);
          }
          // If B is an interop type annotation, the type must be identical
          // to the declared type, modulo checkedness.  So it is OK to
          // compute the array bounds based on the original type.
          return ArrayExprBounds(DR);
        }

        if (DR->getType()->isFunctionType()) {
          // Only function decl refs should have function type
          assert(isa<FunctionDecl>(DR->getDecl()));
          return CreateBoundsEmpty();
        }
        Expr *AddrOf = CreateAddressOfOperator(DR);
        return CreateSingleElementBounds(AddrOf);
      }
      case Expr::UnaryOperatorClass: {
        UnaryOperator *UO = dyn_cast<UnaryOperator>(E);
        if (!UO) {
          llvm_unreachable("unexpected cast failure");
          return CreateBoundsInferenceError();
        }
        if (UO->getOpcode() == UnaryOperatorKind::UO_Deref)
          return RValueBounds(UO->getSubExpr());
        else {
          llvm_unreachable("unexpected lvalue unary operator");
          return CreateBoundsInferenceError();
        }
      }
      case Expr::ArraySubscriptExprClass: {
        //  e1[e2] is a synonym for *(e1 + e2).  The bounds are
        // the bounds of e1 + e2, which reduces to the bounds
        // of whichever subexpression has pointer type.
        ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(E);
        if (!AS) {
          llvm_unreachable("unexpected cast failure");
          return CreateBoundsInferenceError();
        }
        // getBase returns the pointer-typed expression.
        return RValueBounds(AS->getBase());
      }
      case Expr::MemberExprClass: {
        MemberExpr *ME = dyn_cast<MemberExpr>(E);
        if (!ME) {
          llvm_unreachable("unexpected cast failure");
          return CreateBoundsInferenceError();
        }
        FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl());
        if (!FD)
          return CreateBoundsInferenceError();
        if (!SemaRef.CheckIsNonModifying(ME->getBase(),
                                         Sema::NonModifyingContext::NMC_Unknown,
                                         /*ReportError=*/false))
          return CreateBoundsNotAllowedYet();

        if (ME->getType()->isArrayType()) {
          // Declared bounds override the bounds based on the array type.
          BoundsExpr *B = FD->getBoundsExpr();
          if (B && !B->isInteropTypeAnnotation()) {
            B = SemaRef.MakeMemberBoundsConcrete(ME->getBase(), ME->isArrow(), B);
            if (B->isElementCount() || B->isByteCount()) {
              Expr *Base = CreateImplicitCast(Context.getDecayedType(E->getType()),
                                              CastKind::CK_ArrayToPointerDecay,
                                              E);
              return ExpandToRange(Base, B);
            } else
              return B;
          }

          // If B is an interop type annotation, the type must be identical
          // to the declared type, modulo checkedness.  So it is OK to
          // compute the array bounds based on the original type.
          return ArrayExprBounds(ME);
        }

        // It is an error for a member to have function type
        if (ME->getType()->isFunctionType())
          return CreateBoundsInferenceError();

        // If E is an L-value, the ME must be an L-value too.
        if (ME->isRValue()) {
          llvm_unreachable("unexpected MemberExpr r-value");
          return CreateBoundsInferenceError();
        }

        Expr *AddrOf = CreateAddressOfOperator(ME);
        return CreateSingleElementBounds(AddrOf);
      }
      case Expr::ImplicitCastExprClass: {
        ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E);
        if (!ICE) {
          llvm_unreachable("unexpected cast failure");
          return CreateBoundsInferenceError();
        }
        // An LValueBitCast adjusts the type of the lvalue, but
        // the bounds are not changed.
        // TODO: when we add relative alignment support, we may need
        // to adjust the relative alignment of the bounds.
        if (ICE->getCastKind() == CastKind::CK_LValueBitCast)
          return LValueBounds(ICE->getSubExpr());
         return CreateBoundsAlwaysUnknown();
      }
      // TODO: these cases need CurrentExprValue to be implemented to express
      // the bounds.
      case Expr::CompoundLiteralExprClass:
      case Expr::StringLiteralClass:
        return CreateBoundsAllowedButNotComputed();
      default:
        return CreateBoundsAlwaysUnknown();
      }
    }

    // Given a Ptr type or a bounds-safe interface type, create the
    // bounds implied by the type.
    BoundsExpr *CreateTypeBasedBounds(QualType Ty, bool IsParam) {
      // If the target value v is a Ptr type, it has bounds(v, v + 1), unless
      // it is a function pointer type, in which case it has no required
      // bounds.
      if (Ty->isCheckedPointerPtrType()) {
        if (Ty->isFunctionPointerType())
          return CreateBoundsEmpty();
        else return CreateSingleElementBounds();
      } else if (Ty->isCheckedArrayType() && IsParam) {
        return CreateBoundsForArrayType(Ty);
      } else if (Ty->isCheckedPointerNtArrayType()) {
        // Null-terminated pointers get a zero element bounds.
        return Context.getPrebuiltCountZero();
      }

      return CreateBoundsEmpty();
    }

    // This is a specialized version of CreateTypeBasedBounds that
    // lets us avoid allocating an intermediate count bounds expression.
    BoundsExpr *CreateTypeBasedBounds(Expr *E, QualType Ty, bool IsParam,
                                      bool IsBoundsSafeInterface) {
      // If the target value v is a Ptr type, it has bounds(v, v + 1), unless
      // it is a function pointer type, in which case it has no required
      // bounds.
      if (Ty->isCheckedPointerPtrType()) {
        if (Ty->isFunctionPointerType())
          return CreateBoundsEmpty();
        Expr *Base;
        if (E->isLValue()) {
          ImplicitCastExpr *ICE = CreateImplicitCast(Ty, CastKind::CK_LValueToRValue, E);
          ICE->setBoundsSafeInterface(IsBoundsSafeInterface);
          Base = ICE;
        } else
          Base = E;
        return CreateSingleElementBounds(Base);
      } else if (Ty->isCheckedArrayType() && IsParam) {
        assert(IsBoundsSafeInterface && "unexpected checked array type for parameter");
        assert(E->isLValue());
        BoundsExpr *BE = CreateBoundsForArrayType(Ty);
        ImplicitCastExpr *Base = CreateImplicitCast(Ty, CastKind::CK_LValueToRValue, E);
        Base->setBoundsSafeInterface(IsBoundsSafeInterface);
        return ExpandToRange(Base, BE);
      } else if (Ty->isCheckedPointerNtArrayType()) {
        ImplicitCastExpr *Base = CreateImplicitCast(Ty, CastKind::CK_LValueToRValue, E);
        return ExpandToRange(Base, Context.getPrebuiltCountZero());
      }
   
       return CreateBoundsEmpty();
    }

    // Compute bounds for the target of an lvalue.  Values assigned through
    // the lvalue must satisfy these bounds.   Values read through the
    // lvalue will meet these bounds.
    BoundsExpr *LValueTargetBounds(Expr *E) {
      if (!E->isLValue()) return CreateBoundsInferenceError();
      // TODO: handle side effects within E
      E = E->IgnoreParens();
      QualType QT = E->getType();

      // The type here cannot ever be an array type, as these are dealt with
      // by an array conversion, not an lvalue conversion. The bounds for an
      // array conversion are the same as the lvalue bounds of the
      // array-typed expression.
      assert(!QT->isArrayType() &&
             "Unexpected Array-typed lvalue in LValueTargetBounds");
      if (QT->isCheckedPointerPtrType()) {
        bool IsParam = false;
        if (DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E))
          IsParam = isa<ParmVarDecl>(DR->getDecl());

        return CreateTypeBasedBounds(E, QT,/*IsParam=*/IsParam,
                                     /*IsBoundsSafeInterface="*/false);
      }

      switch (E->getStmtClass()) {
        case Expr::DeclRefExprClass: {
          DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
          if (!DR) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }
          VarDecl *D = dyn_cast<VarDecl>(DR->getDecl());
          if (!D)
            return CreateBoundsInferenceError();

          BoundsExpr *B = D->getBoundsExpr();

          if (!B || B->isUnknown())
            return CreateBoundsAlwaysUnknown();

          if (B->isInteropTypeAnnotation())
            // TODO: eventually we need to support an interop type annotation
            // with a bounds declaration too.  For now, we can't have that, so
            // we infer bounds based on the type and do not check to see if
            // the programmer declared bounds.
            return CreateTypeBasedBounds(E, B->getType(),
                                         /*IsParam=*/isa<ParmVarDecl>(D),
                                         /*IsBoundsSafeInterface=*/true);

           Expr *Base = CreateImplicitCast(QT, CastKind::CK_LValueToRValue, E);
           return ExpandToRange(Base, B);
        }
        case Expr::UnaryOperatorClass: {
          UnaryOperator *UO = dyn_cast<UnaryOperator>(E);
          if (!UO) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }
          // Currently, we don't know the bounds of a pointer returned
          // by a pointer dereference, unless it is a _Ptr type (handled
          // earlier) or an _Nt_array_ptr.
          if (UO->getOpcode() == UnaryOperatorKind::UO_Deref &&
              UO->getType()->isCheckedPointerNtArrayType())
              return CreateTypeBasedBounds(UO, UO->getType(), false, false);
          return CreateBoundsAlwaysUnknown();
        }
        case Expr::ArraySubscriptExprClass: {
          //  e1[e2] is a synonym for *(e1 + e2).  The bounds are
          // the bounds of e1 + e2, which reduces to the bounds
          // of whichever subexpression has pointer type.
          ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(E);
          if (!AS) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }
          // Currently, we don't know the bounds of a pointer returned
          // by a subscripting operation, unless it is a _Ptr type (handled
          // earlier) or an _Nt_array_ptr.
          if (AS->getType()->isCheckedPointerNtArrayType())
            return CreateTypeBasedBounds(AS, AS->getType(), false, false);
          return CreateBoundsAlwaysUnknown();
        }
        case Expr::MemberExprClass: {
          MemberExpr *M = dyn_cast<MemberExpr>(E);
          if (!M) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }

          FieldDecl *F = dyn_cast<FieldDecl>(M->getMemberDecl());
          if (!F)
            return CreateBoundsInferenceError();

          BoundsExpr *B = F->getBoundsExpr();
          if (!B || B->isUnknown())
            return CreateBoundsAlwaysUnknown();

          Expr *MemberBaseExpr = M->getBase();
          if (!SemaRef.CheckIsNonModifying(MemberBaseExpr,
                                         Sema::NonModifyingContext::NMC_Unknown,
              /*ReportError=*/false))
            return CreateBoundsNotAllowedYet();

          if (B->isInteropTypeAnnotation())
            // TODO: eventually we need to support an interop type annotation
            // with a bounds declaration too.  For now, we can't have that, so
            // we infer bounds based on the type and do not check to see if
            // the programmer declared bounds.
            return CreateTypeBasedBounds(MemberBaseExpr, B->getType(),
                                         /*IsParam=*/false,
                                         /*IsInteropTypeAnnotation=*/true);

          B = SemaRef.MakeMemberBoundsConcrete(MemberBaseExpr, M->isArrow(), B);
          if (!B)
            return CreateBoundsInferenceError();

          if (B->isElementCount() || B->isByteCount()) {
             Expr *MemberRValue;
            if (M->isLValue())
              MemberRValue = CreateImplicitCast(QT, CastKind::CK_LValueToRValue,
                                                E);
            else
              MemberRValue = M;
            return ExpandToRange(MemberRValue, B);
          }
          return B;
        }
        case Expr::ImplicitCastExprClass: {
          ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(E);
          if (!ICE) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }
          if (ICE->getCastKind() == CastKind::CK_LValueBitCast)
            return LValueTargetBounds(ICE->getSubExpr());
          return CreateBoundsAlwaysUnknown();
        }
        default:
          return CreateBoundsAlwaysUnknown();
      }
    }

    // Compute the bounds of a cast operation that produces an rvalue.
    BoundsExpr *RValueCastBounds(CastKind CK, Expr *E) {
      switch (CK) {
        case CastKind::CK_BitCast:
        case CastKind::CK_DynamicPtrBounds:
        case CastKind::CK_AssumePtrBounds:
        case CastKind::CK_NoOp:
        case CastKind::CK_NullToPointer:
        // Truncation or widening of a value does not affect its bounds.
        case CastKind::CK_IntegralToPointer:
        case CastKind::CK_PointerToIntegral:
        case CastKind::CK_IntegralCast:
        case CastKind::CK_IntegralToBoolean:
        case CastKind::CK_BooleanToSignedIntegral:
          return RValueBounds(E);
        case CastKind::CK_LValueToRValue:
          return LValueTargetBounds(E);
        case CastKind::CK_ArrayToPointerDecay:
          return LValueBounds(E);
        default:
          return CreateBoundsAlwaysUnknown();
      }
    }

    // Compute the bounds of an expression that produces an rvalue.
    BoundsExpr *RValueBounds(Expr *E) {
      if (!E->isRValue()) return CreateBoundsInferenceError();

      E = E->IgnoreParens();

      // Null Ptrs always have bounds(any)
      // This is the correct way to detect all the different ways that
      // C can make a null ptr.
      if (E->isNullPointerConstant(Context, Expr::NPC_NeverValueDependent)) {
        return CreateBoundsAny();
      }

      switch (E->getStmtClass()) {
        case Expr::BoundsCastExprClass: {
          CastExpr *CE = dyn_cast<CastExpr>(E);
          if (!E) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }

          Expr *subExpr = CE->getSubExpr();
          BoundsExpr *Bounds = CE->getBoundsExpr();

          Bounds = ExpandToRange(subExpr, Bounds);
          return Bounds;
        }
        case Expr::ImplicitCastExprClass:
        case Expr::CStyleCastExprClass: {
          CastExpr *CE = dyn_cast<CastExpr>(E);
          if (!E) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }
          // Casts to _Ptr narrow the bounds.  If the cast to
          // _Ptr is invalid, that will be diagnosed separately.
          if (E->getType()->isCheckedPointerPtrType())
            return CreateTypeBasedBounds(E, E->getType(), false, false);
          return RValueCastBounds(CE->getCastKind(), CE->getSubExpr());
        }
        case Expr::UnaryOperatorClass: {
          UnaryOperator *UO = dyn_cast<UnaryOperator>(E);
          if (!UO) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }
          UnaryOperatorKind Op = UO->getOpcode();

          // `*e` is not an r-value.
          if (Op == UnaryOperatorKind::UO_Deref) {
            llvm_unreachable("unexpected dereference expression in RValue Bounds inference");
            return CreateBoundsInferenceError();
          }

          // `!e` has empty bounds
          if (Op == UnaryOperatorKind::UO_LNot)
            return CreateBoundsEmpty();

          Expr *SubExpr = UO->getSubExpr();

          // `&e` has the bounds of `e`.
          // `e` is an lvalue, so its bounds are its lvalue bounds.
          if (Op == UnaryOperatorKind::UO_AddrOf) {

            // Functions have bounds corresponding to the empty range
            if (SubExpr->getType()->isFunctionType())
              return CreateBoundsEmpty();

            return LValueBounds(SubExpr);
          }

          // `++e`, `e++`, `--e`, `e--` all have bounds of `e`.
          // `e` is an LValue, so its bounds are its lvalue target bounds.
          if (UnaryOperator::isIncrementDecrementOp(Op))
            return LValueTargetBounds(SubExpr);

          // `+e`, `-e`, `~e` all have bounds of `e`. `e` is an RValue.
          if (Op == UnaryOperatorKind::UO_Plus ||
              Op == UnaryOperatorKind::UO_Minus ||
              Op == UnaryOperatorKind::UO_Not)
            return RValueBounds(SubExpr);

          // We cannot infer the bounds of other unary operators
          return CreateBoundsAlwaysUnknown();
        }
        case Expr::BinaryOperatorClass:
        case Expr::CompoundAssignOperatorClass: {
          BinaryOperator *BO = dyn_cast<BinaryOperator>(E);
          if (!BO) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }
          Expr *LHS = BO->getLHS();
          Expr *RHS = BO->getRHS();
          BinaryOperatorKind Op = BO->getOpcode();

          // Floating point expressions have empty bounds
          if (BO->getType()->isFloatingType())
            return CreateBoundsEmpty();

          // `e1 = e2` has the bounds of `e2`. `e2` is an RValue.
          if (Op == BinaryOperatorKind::BO_Assign)
            return RValueBounds(RHS);

          // `e1, e2` has the bounds of `e2`. Both `e1` and `e2`
          // are RValues.
          if (Op == BinaryOperatorKind::BO_Comma)
            return RValueBounds(RHS);

          // Compound Assignments function like assignments mostly,
          // except the LHS is an L-Value, so we'll use its lvalue target bounds
          bool IsCompoundAssignment = false;
          if (BinaryOperator::isCompoundAssignmentOp(Op)) {
            Op = BinaryOperator::getOpForCompoundAssignment(Op);
            IsCompoundAssignment = true;
          }

          // Pointer arithmetic.
          //
          // `p + i` has the bounds of `p`. `p` is an RValue.
          // `p += i` has the lvalue target bounds of `p`. `p` is an LValue. `p += i` is an RValue
          // same applies for `-` and `-=` respectively
          if (LHS->getType()->isPointerType() &&
              RHS->getType()->isIntegerType() &&
              BinaryOperator::isAdditiveOp(Op)) {
            return IsCompoundAssignment ?
              LValueTargetBounds(LHS) : RValueBounds(LHS);
          }
          // `i + p` has the bounds of `p`. `p` is an RValue.
          // `i += p` has the bounds of `p`. `p` is an RValue.
          if (LHS->getType()->isIntegerType() &&
              RHS->getType()->isPointerType() &&
              Op == BinaryOperatorKind::BO_Add) {
            return RValueBounds(RHS);
          }
          // `e - p` has empty bounds, regardless of the bounds of p.
          // `e -= p` has empty bounds, regardless of the bounds of p.
          if (RHS->getType()->isPointerType() &&
              Op == BinaryOperatorKind::BO_Sub) {
            return CreateBoundsEmpty();
          }

          // Arithmetic on integers with bounds.
          //
          // `e1 @ e2` has the bounds of whichever of `e1` or `e2` has bounds.
          // if both `e1` and `e2` have bounds, then they must be equal.
          // Both `e1` and `e2` are RValues
          //
          // `e1 @= e2` has the bounds of whichever of `e1` or `e2` has bounds.
          // if both `e1` and `e2` have bounds, then they must be equal.
          // `e1` is an LValue, its bounds are the lvalue target bounds.
          // `e2` is an RValue
          //
          // @ can stand for: +, -, *, /, %, &, |, ^, >>, <<
          if (LHS->getType()->isIntegerType() &&
              RHS->getType()->isIntegerType() &&
              (BinaryOperator::isAdditiveOp(Op) ||
               BinaryOperator::isMultiplicativeOp(Op) ||
               BinaryOperator::isBitwiseOp(Op) ||
               BinaryOperator::isShiftOp(Op))) {
            BoundsExpr *LHSBounds = IsCompoundAssignment ?
              LValueTargetBounds(LHS) : RValueBounds(LHS);
            BoundsExpr *RHSBounds = RValueBounds(RHS);
            if (LHSBounds->isUnknown() && !RHSBounds->isUnknown())
              return RHSBounds;
            if (!LHSBounds->isUnknown() && RHSBounds->isUnknown())
              return LHSBounds;
            if (!LHSBounds->isUnknown() && !RHSBounds->isUnknown()) {
              // TODO: Check if LHSBounds and RHSBounds are equal.
              // if so, return one of them. If not, return bounds(unknown)
              return CreateBoundsAlwaysUnknown();
            }
            if (LHSBounds->isUnknown() && RHSBounds->isUnknown())
              return CreateBoundsEmpty();
          }

          // Comparisons and Logical Ops
          //
          // `e1 @ e2` have empty bounds if @ is:
          // ==, !=, <=, <, >=, >, &&, ||
          if (BinaryOperator::isComparisonOp(Op) ||
              BinaryOperator::isLogicalOp(Op)) {
            return CreateBoundsEmpty();
          }

          // All Other Binary Operators we don't know how to deal with
          return CreateBoundsEmpty();
        }
        case Expr::CallExprClass: {
          const CallExpr *CE = dyn_cast<CallExpr>(E);
          if (!CE) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsInferenceError();
          }

          BoundsExpr *ReturnBounds = nullptr;
          if (E->getType()->isCheckedPointerPtrType())
            ReturnBounds = CreateSingleElementBounds();
          else {
            // Get the function prototype, where the abstract function return
            // bounds are kept. The callee is always a function pointer.
            const PointerType *PtrTy =
              CE->getCallee()->getType()->getAs<PointerType>();
            assert(PtrTy != nullptr);
            const FunctionProtoType *CalleeTy =
              PtrTy->getPointeeType()->getAs<FunctionProtoType>();
            if (!CalleeTy)
              // K&R functions have no prototype, and we cannot perform
              // inference on them, so we return bounds(unknown) for their results.
              return CreateBoundsAlwaysUnknown();

            BoundsExpr *FunBounds =
              const_cast<BoundsExpr *>(CalleeTy->getReturnBounds());
            if (!FunBounds)
              // This function has no return bounds
              return CreateBoundsAlwaysUnknown();

            // TODO:handle interop type annotation on return bounds
            // Github issue #205.  We have no way of rerepresenting
            // CurrentExprValue in the IR yet.
            if (FunBounds->isInteropTypeAnnotation())
              return CreateBoundsAllowedButNotComputed();

            ArrayRef<Expr *> ArgExprs =
              llvm::makeArrayRef(const_cast<Expr**>(CE->getArgs()),
                                 CE->getNumArgs());

            // Concretize Call Bounds with argument expressions.
            // We can only do this if the argument expressions are non-modifying
            ReturnBounds =
              SemaRef.ConcretizeFromFunctionTypeWithArgs(FunBounds, ArgExprs,
                               Sema::NonModifyingContext::NMC_Function_Return);
            // If concretization failed, this means we tried to substitute with
            // a non-modifying expression, which is not allowed by the
            // specification.
            if (!ReturnBounds)
              return CreateBoundsInferenceError();
          }

          // Currently we cannot yet concretize function bounds of the forms
          // count(e) or byte_count(e) becuase we need a way of referring
          // to the function's return value which we currently lack in the
          // general case.
          if (ReturnBounds->isElementCount() ||
              ReturnBounds->isByteCount())
            return CreateBoundsAllowedButNotComputed();

          return ReturnBounds;
        }
        case Expr::ConditionalOperatorClass:
        case Expr::BinaryConditionalOperatorClass:
          // TODO: infer correct bounds for conditional operators
          return CreateBoundsAllowedButNotComputed();
        default:
          // All other cases are unknowable
          return CreateBoundsAlwaysUnknown();
      }
    }
  };
}

Expr *Sema::GetArrayPtrDereference(Expr *E, QualType &Result) {
  assert(E->isLValue());
  E = E->IgnoreParens();
  switch (E->getStmtClass()) {
    case Expr::DeclRefExprClass:
    case Expr::MemberExprClass:
    case Expr::CompoundLiteralExprClass:
      return nullptr;
    case Expr::UnaryOperatorClass: {
      UnaryOperator *UO = dyn_cast<UnaryOperator>(E);
      if (!UO) {
        llvm_unreachable("unexpected cast failure");
        return nullptr;
      }
      if (UO->getOpcode() == UnaryOperatorKind::UO_Deref &&
          UO->getSubExpr()->getType()->isCheckedPointerArrayType()) {
        Result = UO->getSubExpr()->getType();
        return E;
      }

      return nullptr;
    }

    case Expr::ArraySubscriptExprClass: {
      // e1[e2] is a synonym for *(e1 + e2).
      ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(E);
      if (!AS) {
        llvm_unreachable("unexpected cast failure");
        return nullptr;
      }
      // An important invariant for array types in Checked C is that all
      // dimensions of a multi-dimensional array are either checked or
      // unchecked.  This ensures that the intermediate values for
      // multi-dimensional array accesses have checked type and preserve
      //  the "checkedness" of the outermost array.

      // getBase returns the pointer-typed expression.
      if (AS->getBase()->getType()->isCheckedPointerArrayType()) {
        Result = AS->getBase()->getType();
        return E;
      }

      return nullptr;
    }
    case Expr::ImplicitCastExprClass: {
      ImplicitCastExpr *IC = dyn_cast<ImplicitCastExpr>(E);
      if (!IC) {
        llvm_unreachable("unexpected cast failure");
        return nullptr;
      }
      if (IC->getCastKind() == CK_LValueBitCast)
        return GetArrayPtrDereference(IC->getSubExpr(), Result);
      return nullptr;
    }
    default: {
      llvm_unreachable("unexpected lvalue expression");
      return nullptr;
    }
  }
}

BoundsExpr *Sema::InferLValueBounds(Expr *E) {
  return BoundsInference(*this).LValueBounds(E);
}

BoundsExpr *Sema::CreateTypeBasedBounds(QualType QT, bool IsParam) {
  return BoundsInference(*this).CreateTypeBasedBounds(QT, IsParam);
}

BoundsExpr *Sema::InferLValueTargetBounds(Expr *E) {
  return BoundsInference(*this).LValueTargetBounds(E);
}

BoundsExpr *Sema::InferRValueBounds(Expr *E) {
  return BoundsInference(*this).RValueBounds(E);
}

BoundsExpr *Sema::CreateCountForArrayType(QualType QT) {
  return BoundsInference(*this).CreateBoundsForArrayType(QT);
}

BoundsExpr *Sema::ExpandToRange(Expr *Base, BoundsExpr *B) {
  return BoundsInference(*this).ExpandToRange(Base, B);
}

BoundsExpr *Sema::ExpandToRange(VarDecl *D, BoundsExpr *B) {
  QualType QT = D->getType();
  ExprResult ER = BuildDeclRefExpr(D, QT,
                                   clang::ExprValueKind::VK_LValue, SourceLocation());
  if (ER.isInvalid())
    return nullptr;
  Expr *Base = ER.get();
  BoundsInference BI(*this);
  if (!QT->isArrayType())
    Base = BI.CreateImplicitCast(QT, CastKind::CK_LValueToRValue, Base);
  return BI.ExpandToRange(Base, B);
}


namespace {
  class CheckBoundsDeclarations :
    public RecursiveASTVisitor<CheckBoundsDeclarations> {
  private:
    Sema &S;
    bool DumpBounds;

    void DumpAssignmentBounds(raw_ostream &OS, BinaryOperator *E,
                              BoundsExpr *LValueTargetBounds,
                              BoundsExpr *RHSBounds) {
      OS << "\n";
      E->dump(OS);
      if (LValueTargetBounds) {
        OS << "Target Bounds:\n";
        LValueTargetBounds->dump(OS);
      }
      if (RHSBounds) {
        OS << "RHS Bounds:\n ";
        RHSBounds->dump(OS);
      }
    }

    void DumpInitializerBounds(raw_ostream &OS, VarDecl *D,
                               BoundsExpr *Target, BoundsExpr *B) {
      OS << "\n";
      D->dump(OS);
      OS << "Declared Bounds:\n";
      Target->dump(OS);
      OS << "Initializer Bounds:\n ";
      B->dump(OS);
    }

    void DumpExpression(raw_ostream &OS, Expr *E) {
      OS << "\n";
      E->dump(OS);
    }

    // Add bounds check to an lvalue expression, if it is an Array_ptr
    // dereference.  The caller has determined that the lvalue is being
    // used in a way that requies a bounds check if the lvalue is an
    // _Array_ptr or _Nt_array_ptr dereference.  The lvalue uses are to read
    // or write memory or as the base expression of a member reference.
    //
    // If the Array_ptr has unknown bounds, this is a compile-time error.
    // Generate an error message and set the bounds to an invalid bounds
    // expression.
    bool AddBoundsCheck(Expr *E, bool IsOnlyMemoryRead) {
      assert(E->isLValue());
      bool NeedsBoundsCheck = false;
      QualType PtrType;
      if (Expr *Deref = S.GetArrayPtrDereference(E, PtrType)) {
        NeedsBoundsCheck = true;
        BoundsExpr *LValueBounds = S.InferLValueBounds(E);
        BoundsCheckKind Kind = BCK_Normal;
        if (IsOnlyMemoryRead && PtrType->isCheckedPointerNtArrayType())
          Kind = BCK_NullTermRead;
        if (LValueBounds->isUnknown()) {
          S.Diag(E->getLocStart(), diag::err_expected_bounds) << E->getSourceRange();
          LValueBounds = S.CreateInvalidBoundsExpr();
        }
        if (UnaryOperator *UO = dyn_cast<UnaryOperator>(Deref)) {
          assert(!UO->hasBoundsExpr());
          UO->setBoundsExpr(LValueBounds);
          UO->setBoundsCheckKind(Kind);
        } else if (ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(Deref)) {
          assert(!AS->hasBoundsExpr());
          AS->setBoundsExpr(LValueBounds);
          AS->setBoundsCheckKind(Kind);
        } else
          llvm_unreachable("unexpected expression kind");
      }
      return NeedsBoundsCheck;
    }

    // Add bounds check to the base expression of a member reference, if the
    // base expression is an Array_ptr dereference.  Such base expressions
    // always need bounds checks, even though their lvalues are only used for an
    // address computation.
    bool AddMemberBaseBoundsCheck(MemberExpr *E) {
      Expr *Base = E->getBase();
      // E.F
      if (!E->isArrow()) {
        // The base expression only needs a bounds check if it is an lvalue.
        if (Base->isLValue())
          return AddBoundsCheck(Base, false);
        return false;
      }

      // E->F.  This is equivalent to (*E).F.
      if (Base->getType()->isCheckedPointerArrayType()) {
        BoundsExpr *Bounds = S.InferRValueBounds(Base);
        if (Bounds->isUnknown()) {
          S.Diag(Base->getLocStart(), diag::err_expected_bounds) << Base->getSourceRange();
          Bounds = S.CreateInvalidBoundsExpr();
        }
        E->setBoundsExpr(Bounds);
        return true;
      }

      return false;
    }


    // Check that SrcBounds implies that DeclaredBounds are provably
    // true.
    bool CheckBoundsDeclIsProvable(const BoundsExpr *DeclaredBounds,
                                   const BoundsExpr *SrcBounds) {
      // source bounds(any) implies that any other bounds is valid.
      if (SrcBounds->isAny())
        return true;

      // target bounds(unknown) implied by any other bounds.
      if (DeclaredBounds->isUnknown())
        return true;

      return S.Context.EquivalentBounds(DeclaredBounds, SrcBounds);
    }

    // Given an assignment target = e, where target has declared bounds
    // DeclaredBounds and and e has inferred bounds SrcBounds, make sure
    // that SrcBounds implies that DeclaredBounds are provably true.
    void CheckBoundsDeclAtAssignment(SourceLocation ExprLoc, Expr *Target,
                                     BoundsExpr *DeclaredBounds, Expr *Src,
                                     BoundsExpr *SrcBounds) {
      if (S.Diags.isIgnored(diag::warn_bounds_declaration_invalid, ExprLoc))
        return;

      if (!CheckBoundsDeclIsProvable(DeclaredBounds, SrcBounds)) {
        S.Diag(ExprLoc, diag::warn_bounds_declaration_invalid)
          << Sema::BoundsDeclarationCheck::BDC_Assignment << Target
          << Target->getSourceRange() << Src->getSourceRange();
        S.Diag(Target->getExprLoc(), diag::note_declared_bounds)
          << DeclaredBounds << DeclaredBounds->getSourceRange();
        S.Diag(Src->getExprLoc(), diag::note_expanded_inferred_bounds)
          << SrcBounds << Src->getSourceRange();
      }
    }

    // Check that the bounds for an argument imply the expected
    // bounds for the argument.   The expected bounds are computed
    // by substituting the arguments into the bounds expression for
    // the corresponding parameter.
    void CheckBoundsDeclAtCallArg(unsigned ParamNum,
                                  BoundsExpr *ExpectedArgBounds, Expr *Arg,
                                  BoundsExpr *ArgBounds) {
      SourceLocation ArgLoc = Arg->getLocStart();
      if (S.Diags.isIgnored(diag::warn_bounds_declaration_invalid, ArgLoc))
        return;

      BoundsExpr *NormalizedBounds = S.ExpandToRange(Arg, ExpectedArgBounds);
      if (!CheckBoundsDeclIsProvable(NormalizedBounds, ArgBounds)) {
        S.Diag(ArgLoc, diag::warn_argument_bounds_invalid) << (ParamNum + 1)
          << Arg->getSourceRange();
        S.Diag(ArgLoc, diag::note_expected_argument_bounds) << ExpectedArgBounds;
        S.Diag(Arg->getExprLoc(), diag::note_expanded_inferred_bounds)
          << ArgBounds << Arg->getSourceRange();
      }
    }

    // Given an initializer v = e, where v is a variable that has declared
    // bounds DeclaredBounds and and e has inferred bounds SrcBounds, make sure
    // that SrcBounds implies that DeclaredBounds are provably true.
    void CheckBoundsDeclAtInitializer(SourceLocation ExprLoc, VarDecl *D,
                                      BoundsExpr *DeclaredBounds, Expr *Src,
                                      BoundsExpr *SrcBounds) {
      if (S.Diags.isIgnored(diag::warn_bounds_declaration_invalid, ExprLoc))
        return;

      BoundsExpr *NormalizedBounds = S.ExpandToRange(D, DeclaredBounds);
      if (!CheckBoundsDeclIsProvable(NormalizedBounds, SrcBounds)) {
        S.Diag(ExprLoc, diag::warn_bounds_declaration_invalid)
          << Sema::BoundsDeclarationCheck::BDC_Initialization << D
          << D->getLocation() << Src->getSourceRange();
        S.Diag(D->getLocation(), diag::note_declared_bounds)
          << NormalizedBounds << D->getLocation();
        S.Diag(Src->getExprLoc(), diag::note_expanded_inferred_bounds)
          << SrcBounds << Src->getSourceRange();
      }
  }

  public:
    CheckBoundsDeclarations(Sema &S) : S(S),
      DumpBounds(S.getLangOpts().DumpInferredBounds) {}

    bool VisitBinaryOperator(BinaryOperator *E) {
      Expr *LHS = E->getLHS();
      Expr *RHS = E->getRHS();
      QualType LHSType = LHS->getType();
      if (!E->isAssignmentOp())
        return true;

      // Bounds of the target of the lvalue
      BoundsExpr *LHSTargetBounds = nullptr;
      // Bounds of the right-hand side of the assignment
      BoundsExpr *RHSBounds = nullptr;

      if (!E->isCompoundAssignmentOp() &&
          LHSType->isCheckedPointerPtrType() &&
          RHS->getType()->isCheckedPointerPtrType()) {
        // ptr<T> to ptr<T> assignment, no obligation to infer any bounds for either side
      }
      else if (LHSType->isCheckedPointerType() ||
          LHSType->isIntegerType()) {
        // Check that the value being assigned has bounds if the
        // target of the LHS lvalue has bounds.
        LHSTargetBounds = S.InferLValueTargetBounds(LHS);
        if (!LHSTargetBounds->isUnknown()) {
          if (E->isCompoundAssignmentOp())
            RHSBounds = S.InferRValueBounds(E);
          else
            RHSBounds = S.InferRValueBounds(RHS);
          if (RHSBounds->isUnknown()) {
             S.Diag(RHS->getLocStart(),
                    diag::err_expected_bounds_for_assignment)
                    << RHS->getSourceRange();
             RHSBounds = S.CreateInvalidBoundsExpr();
          } else
            CheckBoundsDeclAtAssignment(E->getExprLoc(), LHS, LHSTargetBounds,
                                        RHS, RHSBounds);
        }
      }

      // Check that the LHS lvalue of the assignment has bounds, if it is an
      // lvalue that was produced by dereferencing an _Array_ptr.
      bool LHSNeedsBoundsCheck = false;
      LHSNeedsBoundsCheck = AddBoundsCheck(LHS, false);
      if (DumpBounds && (LHSNeedsBoundsCheck ||
                         (LHSTargetBounds && !LHSTargetBounds->isUnknown())))
        DumpAssignmentBounds(llvm::outs(), E, LHSTargetBounds, RHSBounds);
      return true;
    }

    bool VisitCallExpr(CallExpr *CE) {
      const PointerType *FuncPtrTy = CE->getCallee()->getType()->getAs<PointerType>();
      assert(FuncPtrTy);
      const FunctionType *FuncTy =
        FuncPtrTy->getPointeeType()->getAs<FunctionType>();
      assert(FuncTy);
      const FunctionProtoType *FuncProtoTy = FuncTy->getAs<FunctionProtoType>();
      if (!FuncProtoTy)
        return true;
      if (!FuncProtoTy->hasParamBounds())
        return true;
      unsigned NumParams = FuncProtoTy->getNumParams();
      unsigned NumArgs = CE->getNumArgs();
      unsigned Count = (NumParams < NumArgs) ? NumParams : NumArgs;
      if (false) {
        // TODO: Github issue #374.
        // Need RecursiveASTVisitor.h to visit bounds expressions
        // in functions for this to work.
        CollectPositionalParameters Uses(NumParams);
        Uses.VisitFunctionProtoType(const_cast<FunctionProtoType *>(FuncProtoTy));

        // If arguments are used in bounds expressions and are modifying
        // expressions, issue an error.  TODO: If an argument expression has
        // side-effects, but we can represent the value of the expression in
        // terms of the values of variables before the call, we should use that
        // instead and not issue an error.
        for (unsigned i = 0; i < NumParams; i++)
          if (Uses.IsUsed(i) && i < NumArgs &&
              !S.CheckIsNonModifying(CE->getArg(i),
                              Sema::NonModifyingContext::NMC_Function_Parameter,
                                    false)) {
            S.Diag(CE->getArg(i)->getLocStart(),
                   diag::err_modifying_expr_not_supported)
              << (i + 1) << CE->getArg(i)->getSourceRange();
          }
      }

      ArrayRef<Expr *> ArgExprs = llvm::makeArrayRef(const_cast<Expr**>(CE->getArgs()),
                                                     CE->getNumArgs());
      for (unsigned i = 0; i < Count; i++) {
        if (FuncProtoTy->getParamType(i)->isUncheckedPointerType()) {
          // Skip checking bounds for unchecked pointer parameters, unless
          // the argument was subject to a bounds-safe interface cast.
          ImplicitCastExpr *ICE = dyn_cast<ImplicitCastExpr>(CE->getArg(i));
          if (!(ICE && ICE->getCastKind() == CK_BitCast &&
                ICE->getSubExpr()->getType()->isCheckedPointerType()))
            continue;
        }

        const BoundsExpr *ParamBounds = FuncProtoTy->getParamBounds(i);

        if (!ParamBounds)
          continue;

        if (ParamBounds->isInteropTypeAnnotation())
          ParamBounds = S.CreateTypeBasedBounds(ParamBounds->getType(), true);

        if (ParamBounds->isUnknown())
          continue;

        Expr *Arg = CE->getArg(i);
        BoundsExpr *ArgBounds = S.InferRValueBounds(Arg);
        if (ArgBounds->isUnknown()) {
          S.Diag(Arg->getLocStart(),
                 diag::err_expected_bounds_for_argument) << (i + 1) <<
            Arg->getSourceRange();
          ArgBounds = S.CreateInvalidBoundsExpr();
        }
        else {
          // Concretize parameter bounds with argument expressions. This fails
          // and return null if an argument expression is a modifying
          // expression, but we've already issued an error about about that.
          BoundsExpr *SubstParamBounds =
            S.ConcretizeFromFunctionTypeWithArgs(
              const_cast<BoundsExpr *>(ParamBounds),
              ArgExprs,
              Sema::NonModifyingContext::NMC_Function_Parameter);
          if (SubstParamBounds)
            CheckBoundsDeclAtCallArg(i, SubstParamBounds, Arg, ArgBounds);
        }
      }
      return true;
   }

    // This includes both ImplicitCastExprs and CStyleCastExprs
    bool VisitCastExpr(CastExpr *E) {
      CheckDisallowedFunctionPtrCasts(E);

      CastKind CK = E->getCastKind();
      if (CK == CK_LValueToRValue && !E->getType()->isArrayType()) {
        bool NeedsBoundsCheck = AddBoundsCheck(E->getSubExpr(), true);
        if (NeedsBoundsCheck && DumpBounds)
          DumpExpression(llvm::outs(), E);

        return true;
      }

      // If inferred bounds of e1 are bounds(unknown), compile-time error.
      // If inferred bounds of e1 are bounds(any), no runtime checks.
      // Otherwise, the inferred bounds is bounds(lb, ub).
      // bounds of cast operation is bounds(e2, e3).
      // In code generation, it inserts dynamic_check(lb <= e2 && e3 <= ub).
      if (CK == CK_DynamicPtrBounds) {
        Expr *SubExpr = E->getSubExpr();
        BoundsExpr *SubExprBounds = S.InferRValueBounds(SubExpr);
        BoundsExpr *CastBounds = S.InferRValueBounds(E);

        if (SubExprBounds->isUnknown()) {
          S.Diag(SubExpr->getLocStart(), diag::err_expected_bounds);
        }

        assert(CastBounds);
        E->setCastBoundsExpr(CastBounds);
        E->setSubExprBoundsExpr(SubExprBounds);
      }

      // Casts to _Ptr type must have a source for which we can infer bounds.
      if ((CK == CK_BitCast || CK == CK_IntegralToPointer) &&
          E->getType()->isCheckedPointerPtrType() &&
          !E->getType()->isFunctionPointerType()) {
        BoundsExpr *SrcBounds =
          S.InferRValueBounds(E->getSubExpr());
        if (SrcBounds->isUnknown()) {
          S.Diag(E->getSubExpr()->getLocStart(),
                 diag::err_expected_bounds_for_ptr_cast)
                 << E->getSubExpr()->getSourceRange();
          SrcBounds = S.CreateInvalidBoundsExpr();
        }
        assert(SrcBounds);
        assert(!E->getBoundsExpr());
        E->setBoundsExpr(SrcBounds);

        if (DumpBounds)
          DumpExpression(llvm::outs(), E);
        return true;
      }
      return true;
    }

    // A member expression is a narrowing operator that shrinks the range of
    // memory to which the base refers to a specific member.  We always bounds
    // check the base.  That way we know that the lvalue produced by the
    // member points to a valid range of memory given by
    // (lvalue, lvalue + 1).   The lvalue is interpreted as a pointer to T,
    // where T is the type of the member.
    bool VisitMemberExpr(MemberExpr *E) {
      bool NeedsBoundsCheck = AddMemberBaseBoundsCheck(E);
      if (NeedsBoundsCheck && DumpBounds)
        DumpExpression(llvm::outs(), E);

      return true;
    }

    bool VisitUnaryOperator(UnaryOperator *E) {
      if (!E->isIncrementDecrementOp())
        return true;

      bool NeedsBoundsCheck = AddBoundsCheck(E->getSubExpr(), false);
      if (NeedsBoundsCheck && DumpBounds)
          DumpExpression(llvm::outs(), E);
      return true;
    }

    bool VisitVarDecl(VarDecl *D) {
      if (D->isInvalidDecl())
        return true;

      if (isa<ParmVarDecl>(D))
        return true;

      VarDecl::DefinitionKind defKind = D->isThisDeclarationADefinition();
      if (defKind == VarDecl::DefinitionKind::DeclarationOnly)
        return true;

     if (Expr *Init = D->getInit()) {
       if (Init->getStmtClass() == Expr::BoundsCastExprClass) {
         S.InferRValueBounds(Init);
       }
     }

     // Handle variables with bounds declarations
     BoundsExpr *DeclaredBounds = D->getBoundsExpr();
     if (!DeclaredBounds || DeclaredBounds->isInvalid() ||
         DeclaredBounds->isUnknown())
       return true;

     // TODO: for array types, check that any declared bounds at the point
     // of initialization are true based on the array size.

     // If there is a scalar initializer, check that the initializer meets the bounds
     // requirements for the variable.  For non-scalar types (arrays, structs, and
     // unions), the amount of storage allocated depends on the type, so we don't
     // to check the initializer bounds.
     Expr *Init = D->getInit();
     if (Init && D->getType()->isScalarType()) {
       assert(D->getInitStyle() == VarDecl::InitializationStyle::CInit);
       BoundsExpr *InitBounds = S.InferRValueBounds(Init);
       if (InitBounds->isUnknown()) {
         // TODO: need some place to record the initializer bounds
         S.Diag(Init->getLocStart(), diag::err_expected_bounds_for_initializer)
             << Init->getSourceRange();
         InitBounds = S.CreateInvalidBoundsExpr();
       } else {
         CheckBoundsDeclAtInitializer(D->getLocation(), D, DeclaredBounds,
           Init, InitBounds);
       }
       if (DumpBounds)
         DumpInitializerBounds(llvm::outs(), D, DeclaredBounds, InitBounds);
       // TODO: check that it meets the bounds requirements for the variable.
      }

      return true;
    }

  private:
    // Here we're examining places where a programmer has cast to a
    // checked function pointer type, in order to make sure this cast is
    // safe and valid.
    //
    // 0. This check is only performed on:
    //  a) Casts to function ptr<> types.
    //  b) from the small set of value-preserving casts we allow of function pointers
    //
    // Let's term the outer value (after the cast), E, of type ToType.
    // In the values we're examining, ToType is a ptr<> to a function type.
    //
    // To produce E, the programmer is performing a sequence of casts,
    // both implicit and explicit, and perhaps this sequence includes using
    // addr-of (&) or deref(*).
    //
    // We search this chain, starting at E. We descend through ParenExprs because
    // they are only syntactic, not semantic.
    //
    // 1. If the thing we're casting has a null pointer value, the cast is allowed.
    //
    // 2. If we come across something of ptr<> function type, then one of two things
    //    happens:
    //  a) this type is compatible with ToType, so then the cast is allowed.
    //  b) this type not compatible, so we add an error about casting between incompatible
    //     types and stop descending.
    //    This allows calling functions with originally-declared checked types, 
    //    and local variables with checked function types.
    //
    // 3. If we come across a non-value-preserving cast, then we stop and error because
    //    we are casting between incompatible types. Non-value-preserving casts include
    //    casts that truncate values, and casts that change alignment. An LValueToRValue
    //    cast is also non-value-preserving because it reads memory.
    //  b) we count the unary operators (&) and (*) as cast-like because when applied to a
    //     function pointer they only change the type, not the value.
    //
    // 4. Eventually we may get to the end of the chain of casts. This could end in many
    //    different kinds of expressions and values, but we only allow them if they meet 
    //    all the following reqs:
    //  a) They're DeclRef expressions
    //  b) The Declaration they reference is a Function declaration
    //  c) The type of this function matches the pointee type of ToType
    //
    void CheckDisallowedFunctionPtrCasts(CastExpr *E) {
      // The type of the outer value
      const QualType ToType = E->getType();

      // 0a. We're only looking for casts to checked function ptr<>s.
      if (!ToType->isCheckedPointerPtrType() ||
        !ToType->isFunctionPointerType())
        return;

      // 0b. Always trust casts inserted according to bounds-safe interface rules.
      if (E->isBoundsSafeInterface())
        return;

      // 0c. Check the top-level cast is one that is value-preserving.
      if (!CheckValuePreservingCast(E, ToType)) {
        // it's non-value-preserving, stop
        return;
      }

      const Expr *Needle = E->getSubExpr();
      while (true) {
        Needle = Needle->IgnoreParens();
        QualType NeedleTy = Needle->getType();

        if (Needle->isNullPointerConstant(S.Context, Expr::NPC_NeverValueDependent))
          // 1a. We've got to a null pointer, so this cast is allowed, stop
          return;

        if (const CastExpr *CE = dyn_cast<ImplicitCastExpr>(Needle))
          if (CE->isBoundsSafeInterface())
            // 1b. We've hit a cast inserted according to bounds-safe interface rules.
            return;

        if (NeedleTy->isCheckedPointerPtrType()) {
          // 2. We've found something with ptr<> type, check compatibility.

          bool types_are_compatible = S.Context.typesAreCompatible(ToType, NeedleTy,
                                                                   /*CompareUnqualified=*/false,
                                                                   /*IgnoreBounds=*/false);
          if (!types_are_compatible) {
            // 2b) it is incompatible with ToType, add an error
            S.Diag(Needle->getExprLoc(), diag::err_cast_to_checked_fn_ptr_from_incompatible_type)
              << ToType << NeedleTy << true
              << E->getSourceRange();
          }

          // We can stop here, as we've got back to something of checked ptr<> type. 
          // CheckDisallowedFunctionPtrCasts will be called on any sub-expressions if they
          // are potentially problematic casts to checked ptr<> types. 
          return;
        }

        // If we've found a cast expression...
        if (const CastExpr *NeedleCast = dyn_cast<CastExpr>(Needle)) {
          // 3. check if the cast is value preserving
          if (!CheckValuePreservingCast(NeedleCast, ToType)) {
            // it's non-value-preserving, stop
            return;
          }

          // it is value-preserving, continue descending
          Needle = NeedleCast->getSubExpr();
          NeedleTy = Needle->getType();
          continue;
        }

        // If we've found a unary operator (such as * or &)...
        if (const UnaryOperator *NeedleOp = dyn_cast<UnaryOperator>(Needle)) {
          // 3b. Check if the operator is value-preserving.
          //     Only addr-of (&) and deref (*) are with function pointers
          if (!CheckValuePreservingCastLikeOp(NeedleOp, ToType)) {
            // it's not value-preserving, stop
            return;
          }

          // it is value-preserving, continue descending
          Needle = NeedleOp->getSubExpr();
          NeedleTy = Needle->getType();
          continue;
        }

        // If we've not found a cast or a cast-like operator, 
        // then we stop descending
        break;
      }

      // 4a) Is it a DeclRef?
      const DeclRefExpr *NeedleDeclRef = dyn_cast<DeclRefExpr>(Needle);
      if (!NeedleDeclRef) {
        // Not a DeclRef. Error, stop
        S.Diag(Needle->getExprLoc(), diag::err_cast_to_checked_fn_ptr_must_be_named)
          << ToType << E->getSourceRange();

        return;
      }

      // 4b) Is it a DeclRef to a declared function?
      const FunctionDecl *NeedleFun = dyn_cast<FunctionDecl>(NeedleDeclRef->getDecl());
      if (!NeedleFun) {
        // Not a DeclRef to a Top-Level function. Error, stop.
        S.Diag(Needle->getExprLoc(), diag::err_cast_to_checked_fn_ptr_must_be_named)
          << ToType << E->getSourceRange();

        return;
      }
      
      // 4c) Is the type of the declared referenced function compatible with the 
      //     pointee-type of ToType
      QualType NeedleFunType = NeedleFun->getType();
      if (!S.Context.typesAreCompatible(
        ToType->getPointeeType(), 
        NeedleFunType,
        /*CompareUnqualified=*/false,
        /*IgnoreBounds=*/false)) {
        // The type of the defined function is not compatible with the pointer
        // we're trying to assign it to. Error, stop.

        S.Diag(Needle->getExprLoc(), diag::err_cast_to_checked_fn_ptr_from_incompatible_type)
          << ToType << NeedleFunType << false << E->getSourceRange();

        return;
      }

      // If we get to here, All our checks have passed!
    }

    // This is used in void CheckDisallowedFunctionPtrCasts(Expr*)
    // to find if a cast is value-preserving
    //
    // Other operations might also be, but this algorithm is currently
    // conservative.
    //
    // This will add the required error messages.
    bool CheckValuePreservingCast(const CastExpr *E, const QualType ToType) {
      switch (E->getCastKind())
      {
      case CK_NoOp:
      case CK_NullToPointer:
      case CK_FunctionToPointerDecay:
      case CK_BitCast:
        return true;
      case CK_LValueToRValue: {
        // Reads of checked function pointers are allowed
        QualType ETy = E->getType();
        if (ETy->isCheckedPointerPtrType() &&
          ETy->isFunctionPointerType())
          return true;

        // This reads unchecked memory, which is definitely not value-preserving
        S.Diag(E->getExprLoc(), diag::err_cast_to_checked_fn_ptr_cannot_read_mem)
          << ToType << E->getSourceRange();

        return false;
      }
      default:
        S.Diag(E->getExprLoc(), diag::err_cast_to_checked_fn_ptr_not_value_preserving)
          << ToType << E->getSourceRange();

        return false;
      }
    }

    // This is used in void CheckDisallowedFunctionPtrCasts(Expr*)
    // to find if the thing we just discovered is deref (*) or
    // addr-of (&) operator on a function pointer type.
    // These operations are value perserving.
    //
    // Other operations might also be, but this algorithm is currently
    // conservative.
    //
    // This will add the required error messages
    bool CheckValuePreservingCastLikeOp(const UnaryOperator *E, const QualType ToType) {
      QualType ETy = E->getType();
      QualType SETy = E->getSubExpr()->getType();

      switch (E->getOpcode()) {
      case UO_Deref: {
        // This may be more conservative than necessary.
        bool between_functions = ETy->isFunctionType() && SETy->isFunctionPointerType();

        if (!between_functions) {
          // Add Error Message
          S.Diag(E->getExprLoc(), diag::err_cast_to_checked_fn_ptr_can_only_ref_deref_functions)
            << ToType << 0 << E->getSourceRange();
        }

        return between_functions;
      }
      case UO_AddrOf: {
        // This may be more conservative than necessary.
        bool between_functions = ETy->isFunctionPointerType() && SETy->isFunctionType();
        if (!between_functions) {
          // Add Error Message
          S.Diag(E->getExprLoc(), diag::err_cast_to_checked_fn_ptr_can_only_ref_deref_functions)
            << ToType << 1 << E->getSourceRange();
        }

        return between_functions;
      }
      default:
        S.Diag(E->getExprLoc(), diag::err_cast_to_checked_fn_ptr_not_value_preserving)
          << ToType << E->getSourceRange();

        return false;
      }
    }
  };
}

void Sema::CheckFunctionBodyBoundsDecls(FunctionDecl *FD, Stmt *Body) {
  CheckBoundsDeclarations(*this).TraverseStmt(Body);
}

void Sema::CheckTopLevelBoundsDecls(VarDecl *D) {
  if (!D->isLocalVarDeclOrParm())
    CheckBoundsDeclarations(*this).TraverseVarDecl(D);
}


namespace {
  class NonModifiyingExprSema : public RecursiveASTVisitor<NonModifiyingExprSema> {

  private:
    // Represents which kind of modifying expression we have found
    enum ModifyingExprKind {
      MEK_Assign,
      MEK_Increment,
      MEK_Decrement,
      MEK_Call,
      MEK_Volatile
    };

  public:
    NonModifiyingExprSema(Sema &S, Sema::NonModifyingContext From,
                          bool ReportError) :
      S(S), FoundModifyingExpr(false), ReqFrom(From),
      ReportError(ReportError) {}

    bool isNonModifyingExpr() { return !FoundModifyingExpr; }

    // Assignments are of course modifying
    bool VisitBinAssign(BinaryOperator* E) {
      addError(E, MEK_Assign);
      FoundModifyingExpr = true;

      return true;
    }

    // Assignments are of course modifying
    bool VisitCompoundAssignOperator(CompoundAssignOperator *E) {
      addError(E, MEK_Assign);
      FoundModifyingExpr = true;

      return true;
    }

    // Pre-increment/decrement, Post-increment/decrement
    bool VisitUnaryOperator(UnaryOperator *E) {
      if (E->isIncrementDecrementOp()) {
        addError(E,
          E->isIncrementOp() ? MEK_Increment : MEK_Decrement);
        FoundModifyingExpr = true;
      }

      return true;
    }

    // References to volatile variables
    bool VisitDeclRefExpr(DeclRefExpr *E) {
      QualType RefType = E->getType();
      if (RefType.isVolatileQualified()) {
        addError(E, MEK_Volatile);
        FoundModifyingExpr = true;
      }

      return true;
    }

    // Function Calls are defined as modifying
    bool VisitCallExpr(CallExpr *E) {
      addError(E, MEK_Call);
      FoundModifyingExpr = true;

      return true;
    }


  private:
    Sema &S;
    bool FoundModifyingExpr;
    Sema::NonModifyingContext ReqFrom;
    bool ReportError;

    void addError(Expr *E, ModifyingExprKind Kind) {
      if (ReportError)
        S.Diag(E->getLocStart(), diag::err_not_non_modifying_expr)
          << Kind << ReqFrom << E->getSourceRange();
    }
  };
}

bool Sema::CheckIsNonModifying(Expr *E, NonModifyingContext Req,
                               bool ReportError) {
  NonModifiyingExprSema Checker(*this, Req, ReportError);
  Checker.TraverseStmt(E);

  return Checker.isNonModifyingExpr();
}

void Sema::WarnDynamicCheckAlwaysFails(const Expr *Condition) {
  bool ConditionConstant;
  if (Condition->EvaluateAsBooleanCondition(ConditionConstant, Context)) {
    if (!ConditionConstant) {
      // Dynamic Check always fails, emit warning
      Diag(Condition->getLocStart(), diag::warn_dynamic_check_condition_fail)
        << Condition->getSourceRange();
    }
  }
}
