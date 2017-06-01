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
#include "TreeTransform.h"

using namespace clang;
using namespace sema;

namespace {
  class AbstractBoundsExpr : public TreeTransform<AbstractBoundsExpr> {
    typedef TreeTransform<AbstractBoundsExpr> BaseTransform;
    typedef ArrayRef<DeclaratorChunk::ParamInfo> ParamsInfo;

  private:
    const ParamsInfo Params;

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
          SemaRef.Diag(E->getLocation(),
                       diag::err_out_of_scope_function_type_local);
        else if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
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
    Result = nullptr;
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

    BoundsExpr *CreateBoundsNone() {
      return new (Context) NullaryBoundsExpr(BoundsExpr::Kind::None,
                                             SourceLocation(),
                                             SourceLocation());
    }

    BoundsExpr *CreateBoundsAny() {
      return new (Context) NullaryBoundsExpr(BoundsExpr::Kind::Any,
                                             SourceLocation(),
                                             SourceLocation());
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

    Expr *CreateImplicitCast(QualType Target, CastKind CK, Expr *E) {
      return ImplicitCastExpr::Create(Context, Target, CK, E, nullptr,
                                       ExprValueKind::VK_RValue);
    }

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
        return CreateBoundsNone();

      IntegerLiteral *Size = CreateIntegerLiteral(CAT->getSize());

      CountBoundsExpr *CBE =
         new (Context) CountBoundsExpr(BoundsExpr::Kind::ElementCount,
                                       Size, SourceLocation(),
                                       SourceLocation());
      return CBE;
    }

  private:
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
            return CreateBoundsNone();
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
                                          false);
          return new (Context) RangeBoundsExpr(LowerBound, UpperBound,
                                               SourceLocation(),
                                               SourceLocation());
        }
        case BoundsExpr::Kind::InteropTypeAnnotation:
          return CreateBoundsNone();
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
      if (BE->isNone())
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
      if (!E->isLValue()) return CreateBoundsNone();
      // TODO: handle side effects within E
      E = E->IgnoreParens();
      switch (E->getStmtClass()) {
      case Expr::DeclRefExprClass: {
        DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
        if (!DR) {
          llvm_unreachable("unexpected cast failure");
          return CreateBoundsNone();
        }

        if (DR->getType()->isArrayType())
          return ArrayExprBounds(DR);

        // TODO: distinguish between variable vs. function
        // references.  This should only apply to function
        // references.
        if (DR->getType()->isFunctionType())
          return CreateBoundsNone();

        Expr *AddrOf = CreateAddressOfOperator(DR);
        return CreateSingleElementBounds(AddrOf);
      }
      case Expr::UnaryOperatorClass: {
        UnaryOperator *UO = dyn_cast<UnaryOperator>(E);
        if (!UO) {
          llvm_unreachable("unexpected cast failure");
          return CreateBoundsNone();
        }
        if (UO->getOpcode() == UnaryOperatorKind::UO_Deref)
          return RValueBounds(UO->getSubExpr());
        else {
          llvm_unreachable("unexpected lvalue unary operator");
          return CreateBoundsNone();
        }
      }
      case Expr::ArraySubscriptExprClass: {
        //  e1[e2] is a synonym for *(e1 + e2).  The bounds are
        // the bounds of e1 + e2, which reduces to the bounds
        // of whichever subexpression has pointer type.
        ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(E);
        if (!AS) {
          llvm_unreachable("unexpected cast failure");
          return CreateBoundsNone();
        }
        // getBase returns the pointer-typed expression.
        return RValueBounds(AS->getBase());
      }
      case Expr::MemberExprClass: {
        MemberExpr *ME = dyn_cast<MemberExpr>(E);
        FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl());
        if (!FD)
          return CreateBoundsNone();
        if (!SemaRef.CheckIsNonModifyingExpr(ME->getBase(),
                             Sema::NonModifiyingExprRequirement::NMER_Unknown,
                                           /*ReportError=*/false))
          return CreateBoundsNone();

        if (ME->getType()->isArrayType())
          return ArrayExprBounds(ME);

        // TODO: can a member have a function type, or is it adjusted
        // to be a pointer function type?
        if (ME->getType()->isFunctionType())
          return CreateBoundsNone();

        if (ME->isRValue())
          return CreateBoundsNone();

        Expr *AddrOf = CreateAddressOfOperator(ME);
        return CreateSingleElementBounds(AddrOf);
      }
      // TODO: fill in these cases.
      case Expr::CompoundLiteralExprClass:
        return CreateBoundsAny();
      default:
        return CreateBoundsNone();
      }
    }

    // Compute bounds for the target of an lvalue.  Values assigned through
    // the lvalue must satisfy these bounds.   Values read through the
    // lvalue will meet these bounds.
    BoundsExpr *LValueTargetBounds(Expr *E) {
      assert(E->isLValue());
      // TODO: handle side effects within E
      E = E->IgnoreParens();
      QualType QT = E->getType();

      // If the target value v is a Ptr type, it has bounds(v, v + 1), unless
      // it is a function pointer type, in which case it has no required
      // bounds.
      if (QT->isCheckedPointerPtrType()) {
         if (QT->isFunctionPointerType())
           return CreateBoundsNone();

        Expr *Base = CreateImplicitCast(E->getType(),
                                        CastKind::CK_LValueToRValue, E);
        return CreateSingleElementBounds(Base);
      }

      switch (E->getStmtClass()) {
        case Expr::DeclRefExprClass: {
          DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
          if (!DR) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsNone();
          }
          VarDecl *D = dyn_cast<VarDecl>(DR->getDecl());
          if (!D)
            return CreateBoundsNone();

          BoundsExpr *B = D->getBoundsExpr();
          if (!B || B->isNone())
            return CreateBoundsNone();

           Expr *Base = CreateImplicitCast(QT, CastKind::CK_LValueToRValue, E);
           return ExpandToRange(Base, B);
        }
        case Expr::MemberExprClass: {
          MemberExpr *M = dyn_cast<MemberExpr>(E);
          if (!M) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsNone();
          }

          FieldDecl *F = dyn_cast<FieldDecl>(M->getMemberDecl());
          if (!F)
            return CreateBoundsNone();

          BoundsExpr *B = F->getBoundsExpr();
          if (!B || B->isNone() || B->isInteropTypeAnnotation())
            return CreateBoundsNone();

          Expr *MemberBaseExpr = M->getBase();
          if (!SemaRef.CheckIsNonModifyingExpr(MemberBaseExpr,
              Sema::NonModifiyingExprRequirement::NMER_Unknown,
              /*ReportError=*/false))
            return CreateBoundsNone();
          B = SemaRef.MakeMemberBoundsConcrete(MemberBaseExpr, M->isArrow(), B);
          if (!B)
            return CreateBoundsNone();
          if (B->isElementCount() || B->isByteCount()) {
             Expr *MemberRValue;
            if (M->isLValue())
               MemberRValue = CreateImplicitCast(QT, CastKind::CK_LValueToRValue, E);
            else
              MemberRValue = M;
            return ExpandToRange(MemberRValue, B);
          }
          return B;
        }
        default:
          return CreateBoundsNone();
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
          return CreateBoundsNone();
      }
    }

    // Compute the bounds of an expression that produces an rvalue.
    BoundsExpr *RValueBounds(Expr *E) {
      assert(E->isRValue());
      E = E->IgnoreParens();
      switch (E->getStmtClass()) {
        case Expr::IntegerLiteralClass: {
         IntegerLiteral *Lit = dyn_cast<IntegerLiteral>(E);
         if (!Lit) {
           llvm_unreachable("unexpected cast failure");
           return CreateBoundsNone();
         }
         if (Lit->getValue() == 0)
           return CreateBoundsAny();

         return CreateBoundsNone();
        }
        case Expr::DeclRefExprClass: {
          DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
          if (!DR) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsNone();
          }
          EnumConstantDecl *ECD = dyn_cast<EnumConstantDecl>(DR->getDecl());
          if (ECD->getInitVal() == 0)
            return CreateBoundsAny();

          return CreateBoundsNone();
        }
        case Expr::BoundsCastExprClass: {
          CastExpr *CE = dyn_cast<CastExpr>(E);
          if (!E) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsNone();
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
            return CreateBoundsNone();
          }
          return RValueCastBounds(CE->getCastKind(), CE->getSubExpr());
        }
        case Expr::UnaryOperatorClass: {
          UnaryOperator *UO = dyn_cast<UnaryOperator>(E);
          if (!UO) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsNone();
          }
          UnaryOperatorKind Op = UO->getOpcode();

          if (Op == UnaryOperatorKind::UO_AddrOf) {
            Expr *SubExpr = UO->getSubExpr();
            if (SubExpr->getType()->isFunctionType())
              return CreateBoundsNone();

            return LValueBounds(SubExpr);
          }

          if (UnaryOperator::isIncrementDecrementOp(Op))
            return LValueTargetBounds(UO->getSubExpr());

          if (Op == UnaryOperatorKind::UO_Plus ||
              Op == UnaryOperatorKind::UO_Minus ||
              Op == UnaryOperatorKind::UO_Not)
            return RValueBounds(UO->getSubExpr());
          return CreateBoundsNone();
        }
        case Expr::BinaryOperatorClass:
        case Expr::CompoundAssignOperatorClass: {
          BinaryOperator *BO = dyn_cast<BinaryOperator>(E);
          if (!BO) {
            llvm_unreachable("unexpected cast failure");
            return CreateBoundsNone();
          }
          Expr *LHS = BO->getLHS();
          Expr *RHS = BO->getRHS();
          BinaryOperatorKind Op = BO->getOpcode();

          if (Op == BinaryOperatorKind::BO_Assign)
            return RValueBounds(RHS);

          bool IsCompoundAssignment = false;
          if (BinaryOperator::isCompoundAssignmentOp(Op)) {
            Op = BinaryOperator::getOpForCompoundAssignment(Op);
            IsCompoundAssignment = true;
          }

          // Pointer arithmetic.
          if (LHS->getType()->isPointerType() &&
              RHS->getType()->isIntegerType() &&
              BinaryOperator::isAdditiveOp(Op)) {
            return IsCompoundAssignment ?
              LValueTargetBounds(LHS) : RValueBounds(LHS);
          }
          if (LHS->getType()->isIntegerType() &&
              RHS->getType()->isPointerType() &&
              BinaryOperator::isAdditiveOp(Op)) {
            assert(!IsCompoundAssignment);
            return RValueBounds(RHS);
          }

          // Arithmetic on integers with bounds.
          if (LHS->getType()->isIntegerType() &&
              RHS->getType()->isIntegerType() &&
              (BinaryOperator::isAdditiveOp(Op) ||
               BinaryOperator::isMultiplicativeOp(Op) ||
               BinaryOperator::isBitwiseOp(Op))) {
            BoundsExpr *LHSBounds = IsCompoundAssignment ?
              LValueTargetBounds(LHS) : RValueBounds(LHS);
            BoundsExpr *RHSBounds = RValueBounds(RHS);
            if (LHSBounds->isNone() && !RHSBounds->isNone())
              return RHSBounds;
            if (!LHSBounds->isNone() && RHSBounds->isNone())
              return LHSBounds;
            // TODO: check if both LHS and RHS have bounds and
            // indicate cause of failure to have bounds.
          }
          return CreateBoundsNone();
        }
        case Expr::CallExprClass:
        case Expr::ConditionalOperatorClass:
        case Expr::BinaryConditionalOperatorClass:
          return CreateBoundsAny();
        default:
          return CreateBoundsNone();
      }
    }
  };
}

Expr *Sema::GetArrayPtrDereference(Expr *E) {
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
          UO->getSubExpr()->getType()->isCheckedPointerArrayType())
        return E;

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
      if (AS->getBase()->getType()->isCheckedPointerArrayType())
        return E;

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

BoundsExpr *Sema::InferLValueTargetBounds(Expr *E) {
  return BoundsInference(*this).LValueTargetBounds(E);
}

BoundsExpr *Sema::InferRValueBounds(Expr *E) {
  return BoundsInference(*this).RValueBounds(E);
}

BoundsExpr *Sema::CreateCountForArrayType(QualType QT) {
  return BoundsInference(*this).CreateBoundsForArrayType(QT);
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
    // Array_ptr dereferences.  The lvalue uses are to read or write memory
    // or as the base expression of a member reference.
    //
    // If the Array_ptr has unknown bounds, this is a compile-time error.
    // Generate an error message and set the bounds to an invalid bounds
    // expression.
    bool AddBoundsCheck(Expr *E) {
      assert(E->isLValue());
      bool NeedsBoundsCheck = false;
      BoundsExpr *LValueBounds = nullptr;
      if (Expr *Deref = S.GetArrayPtrDereference(E)) {
        NeedsBoundsCheck = true;
        LValueBounds = S.InferLValueBounds(E);
        if (LValueBounds->isNone()) {
          S.Diag(E->getLocStart(), diag::err_expected_bounds) << E->getSourceRange();
          LValueBounds = S.CreateInvalidBoundsExpr();
        }
        if (UnaryOperator *UO = dyn_cast<UnaryOperator>(Deref)) {
          assert(!UO->hasBoundsExpr());
          UO->setBoundsExpr(LValueBounds);
        }
        else if (ArraySubscriptExpr *AS = dyn_cast<ArraySubscriptExpr>(Deref)) {
          assert(!AS->hasBoundsExpr());
          AS->setBoundsExpr(LValueBounds);
        } else
          llvm_unreachable("unexpected expression kind");
      }
      return NeedsBoundsCheck;
    }

    // Add bounds check to the base expression of a member reference, if the
    // base expression is an Array_ptr dereference.  Such base expressions
    // always need bounds checka, even though their lvalues are only used for an
    // address computation.
    bool AddMemberBaseBoundsCheck(MemberExpr *E) {
      Expr *Base = E->getBase();
      // E.F
      if (!E->isArrow()) {
        // The base expression only needs a bounds check if it is an lvalue.
        if (Base->isLValue())
          return AddBoundsCheck(Base);
        return false;
      }

      // E->F.  This is equivalent to (*E).F.
      if (Base->getType()->isCheckedPointerArrayType()){
        BoundsExpr *Bounds = S.InferRValueBounds(Base);
        if (Bounds->isNone()) {
          S.Diag(Base->getLocStart(), diag::err_expected_bounds) << Base->getSourceRange();
          Bounds = S.CreateInvalidBoundsExpr();
        }
        E->setBoundsExpr(Bounds);
        return true;
      }

      return false;
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

      // Check that the value being assigned has bounds if the
      // target of the LHS lvalue has bounds.
      if (LHSType->isCheckedPointerType() ||
          LHSType->isIntegerType()) {
        LHSTargetBounds = S.InferLValueTargetBounds(LHS);
        if (!LHSTargetBounds->isNone()) {
          if (E->isCompoundAssignmentOp())
            RHSBounds = S.InferRValueBounds(E);
          else
            RHSBounds = S.InferRValueBounds(RHS);
          if (RHSBounds->isNone()) {
             S.Diag(RHS->getLocStart(), diag::err_expected_bounds_for_assignment) << RHS->getSourceRange();
             RHSBounds = S.CreateInvalidBoundsExpr();
          }
        }
      }

      // Check that the LHS lvalue of the assignment has bounds, if it is an
      // lvalue that was produced by dereferencing an _Array_ptr.
      bool LHSNeedsBoundsCheck = false;
      LHSNeedsBoundsCheck = AddBoundsCheck(LHS);
      if (DumpBounds && (LHSNeedsBoundsCheck ||
                         (LHSTargetBounds && !LHSTargetBounds->isNone())))
        DumpAssignmentBounds(llvm::outs(), E, LHSTargetBounds, RHSBounds);
      return true;
    }

    // This includes both ImplicitCastExprs and CStyleCastExprs
    bool VisitCastExpr(CastExpr *E) {
      CheckDisallowedFunctionPtrCasts(E);

      CastKind CK = E->getCastKind();
      if (CK == CK_LValueToRValue && !E->getType()->isArrayType()) {
        bool NeedsBoundsCheck = AddBoundsCheck(E->getSubExpr());
        if (NeedsBoundsCheck && DumpBounds)
          DumpExpression(llvm::outs(), E);

        return true;
      }

      // If inferred bounds of e1 are bounds(none), compile-time error.
      // If inferred bounds of e1 are bounds(any), no runtime checks.
      // Otherwise, the inferred bounds is bounds(lb, ub).
      // bounds of cast operation is bounds(e2, e3).
      // In code generation, it inserts dynamic_check(lb <= e2 && e3 <= ub).
      if (CK == CK_DynamicPtrBounds) {
        Expr *SubExpr = E->getSubExpr();
        BoundsExpr *SubExprBounds = S.InferRValueBounds(SubExpr);
        BoundsExpr *CastBounds = S.InferRValueBounds(E);

        if (SubExprBounds->isNone()) {
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
        if (SrcBounds->isNone()) {
          S.Diag(E->getSubExpr()->getLocStart(), diag::err_expected_bounds_for_ptr_cast)  << E->getSubExpr()->getSourceRange();
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

      bool NeedsBoundsCheck = AddBoundsCheck(E->getSubExpr());
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

      // D must be a tentative definition or an actual definition.

      if (D->getType()->isCheckedPointerPtrType()) {
        // Make sure that automatic variables are initialized.
        if (D->hasLocalStorage() && !D->hasInit())
          S.Diag(D->getLocation(), diag::err_initializer_expected_for_ptr) << D;

        // Static variables are always initialized to a valid initialization
        // value for bounds, if there is no initializer.
        // * If this is an actual definition, the variable will be initialized
        //   to 0 (a valid value for any bounds).
        // * If this is a tentative definition, the variable will be initialized
        //   to 0 or a valid value by an initializer elsewhere.
        return true;
     }

     if (Expr *Init = D->getInit()) {
       if (Init->getStmtClass() == Expr::BoundsCastExprClass) {
         S.InferRValueBounds(Init);
       }
     }

     // Handle variables with bounds declarations
     BoundsExpr *DeclaredBounds = D->getBoundsExpr();
     if (!DeclaredBounds || DeclaredBounds->isInvalid() ||
         DeclaredBounds->isNone())
       return true;

     // If there is an initializer, check that the initializer meets the bounds
     // requirements for the variable.
     if (Expr *Init = D->getInit()) {
       assert(D->getInitStyle() == VarDecl::InitializationStyle::CInit);
       BoundsExpr *InitBounds = S.InferRValueBounds(Init);
       if (InitBounds->isNone()) {
         // TODO: need some place to record the initializer bounds
         S.Diag(Init->getLocStart(), diag::err_expected_bounds_for_initializer)
             << Init->getSourceRange();
         InitBounds = S.CreateInvalidBoundsExpr();
       }
       if (DumpBounds)
         DumpInitializerBounds(llvm::outs(), D, DeclaredBounds, InitBounds);
       // TODO: check that it meets the bounds requirements for the variable.
      }
      else {
        // Make sure that automatic variables that are not arrays are
        // initialized.
        if (D->hasLocalStorage() && !D->getType()->isArrayType())
          S.Diag(D->getLocation(),
                 diag::err_initializer_expected_with_bounds) << D;
        // Static variables are always initialized to a valid initialization
        // value for bounds, if there is no initializer.  See the prior comment
        // for isCheckedPointerPtrType.
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

      // 0b. Check the top-level cast is one that is value-preserving.
      if (!CheckValuePreservingCast(E, ToType)) {
        // it's non-value-preserving, stop
        return;
      }

      const Expr *Needle = E->getSubExpr();
      while (true) {
        Needle = Needle->IgnoreParens();
        QualType NeedleTy = Needle->getType();

        if (Needle->isNullPointerConstant(S.Context, Expr::NPC_NeverValueDependent))
          // 1. We've got to a null pointer, so this cast is allowed, stop
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
    NonModifiyingExprSema(Sema &S, Sema::NonModifiyingExprRequirement From, bool ReportError) :
      S(S), FoundModifyingExpr(false), ReqFrom(From), ReportError(ReportError) {}

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
    Sema::NonModifiyingExprRequirement ReqFrom;
    bool ReportError;

    void addError(Expr *E, ModifyingExprKind Kind) {
      if (ReportError)
        S.Diag(E->getLocStart(), diag::err_not_non_modifying_expr)
          << Kind << ReqFrom << E->getSourceRange();
    }
  };
}

bool Sema::CheckIsNonModifyingExpr(Expr *E, NonModifiyingExprRequirement Req,
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
