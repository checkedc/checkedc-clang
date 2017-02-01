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
#include "clang/Basic/TargetBuiltins.h"
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

    Expr *CreateImplicitCast(QualType Target, CastKind CK, Expr *E) {
      return ImplicitCastExpr::Create(Context, Target, CK, E, nullptr,
                                       ExprValueKind::VK_RValue);
    }

    Expr *CreateExplicitCast(QualType Target, CastKind CK, Expr *E) {
      return CStyleCastExpr::Create(Context, Target, ExprValueKind::VK_RValue,
                                      CK, E, nullptr, nullptr, SourceLocation(),
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

    BoundsExpr *ExpandToRange(Expr *Base, BoundsExpr *B) {
      assert(Base->isRValue() && "expected rvalue expression");
      BoundsExpr::Kind K = B->getKind();
      switch (K) {
        case BoundsExpr::Kind::ByteCount:
        case BoundsExpr::Kind::ElementCount: {
          CountBoundsExpr *BC = dyn_cast<CountBoundsExpr>(B);
          if (!BC) {
            assert("unexpected cast failure");
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
    BoundsInference(ASTContext &Ctx) : Context(Ctx) {
    }

    BoundsExpr *ArrayVariableBounds(DeclRefExpr *DR) {
      QualType QT = DR->getType();
      const ConstantArrayType *CAT = Context.getAsConstantArrayType(QT);
      if (!CAT)
        return CreateBoundsNone();

      VarDecl *D = dyn_cast<VarDecl>(DR->getDecl());
      if (!D)
        return CreateBoundsNone();

      IntegerLiteral *Size = CreateIntegerLiteral(CAT->getSize());
      Expr *Base = CreateImplicitCast(Context.getDecayedType(QT),
                                      CastKind::CK_ArrayToPointerDecay,
                                      DR);
      CountBoundsExpr CBE = CountBoundsExpr(BoundsExpr::Kind::ElementCount,
                                            Size, SourceLocation(),
                                            SourceLocation());
      return ExpandToRange(Base, &CBE);
    }

    // Compute bounds for an lvalue.  The bounds determine whether
    // it is valid to access memory using the lvalue.  The bounds
    // should be the range of an object in memory or a subrange of
    // an object.
    BoundsExpr *LValueBounds(Expr *E) {
      assert(E->isLValue());
      // TODO: handle side effects within E
      E = E->IgnoreParens();
      switch (E->getStmtClass()) {
      case Expr::DeclRefExprClass: {
        DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
        if (!DR) {
          assert("unexpected cast failure");
          return CreateBoundsNone();
        }

        if (DR->getType()->isArrayType())
          return ArrayVariableBounds(DR);

        if (DR->getType()->isFunctionType())
          return CreateBoundsNone();

        // TODO: bounds for lvalues for variables.
        // Needed for address-taken variables.
        return CreateBoundsNone();
      }
      // TODO: fill in these cases.
      case Expr::UnaryOperatorClass:
      case Expr::ArraySubscriptExprClass:
      case Expr::MemberExprClass:
      case Expr::CompoundLiteralExprClass:
        return CreateBoundsAny();
      default:
        return CreateBoundsNone();
      }
    }

    // Compute bounds for the target of an lvalue.  Values assigned through
    // the lvalue must meet satisfy these bounds.   Values read through the
    // lvalue will meet these bounds.
    BoundsExpr *LValueTargetBounds(Expr *E) {
      assert(E->isLValue());
      // TODO: handle side effects within E
      E = E->IgnoreParens();
      switch (E->getStmtClass()) {
        case Expr::DeclRefExprClass: {
          DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
          if (!DR) {
            assert("unexpected cast failure");
            return CreateBoundsNone();
          }
          VarDecl *D = dyn_cast<VarDecl>(DR->getDecl());
          if (!D)
            return CreateBoundsNone();

          BoundsExpr *B = D->getBoundsExpr();
          if (!B || B->isNone())
            return CreateBoundsNone();

           Expr *Base = CreateImplicitCast(E->getType(),
                                           CastKind::CK_LValueToRValue, E);
           return ExpandToRange(Base, B);
        }
        case Expr::MemberExprClass: {
          MemberExpr *M = dyn_cast<MemberExpr>(E);
          if (!M) {
            assert("unexpected cast failure");
            return CreateBoundsNone();
          }

          FieldDecl *F = dyn_cast<FieldDecl>(M->getMemberDecl());
          if (!F)
            return CreateBoundsNone();

          BoundsExpr *B = F->getBoundsExpr();
          if (!B || B->isNone())
            return CreateBoundsNone();

          Expr *Base = CreateImplicitCast(E->getType(),
                                          CastKind::CK_LValueToRValue, E);
          return ExpandToRange(Base, B);
        }
        default:
          return CreateBoundsNone();
      }
    }

    // Compute the bounds of a cast operation that produces
    // an rvalue.
    BoundsExpr *RValueCastBounds(CastKind CK, Expr *E) {
      switch (CK) {
        case CastKind::CK_BitCast:
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

    // Compute the bounds of an expression that produces
    // an rvalue.
    BoundsExpr *RValueBounds(Expr *E) {
      assert(E->isRValue());
      E = E->IgnoreParens();
      switch (E->getStmtClass()) {
        case Expr::IntegerLiteralClass: {
         IntegerLiteral *Lit = dyn_cast<IntegerLiteral>(E);
         if (!Lit) {
           assert("unexpected cast failure");
           return CreateBoundsNone();
         }
         if (Lit->getValue() == 0)
           return CreateBoundsAny();

         return CreateBoundsNone();
        }
        case Expr::DeclRefExprClass: {
          DeclRefExpr *DR = dyn_cast<DeclRefExpr>(E);
          if (!DR) {
            assert("unexpected cast failure");
            return CreateBoundsNone();
          }
          EnumConstantDecl *ECD = dyn_cast<EnumConstantDecl>(DR->getDecl());
          if (ECD->getInitVal() == 0)
            return CreateBoundsAny();

          return CreateBoundsNone();
        }
        case Expr::ImplicitCastExprClass:
        case Expr::CStyleCastExprClass: {
          CastExpr *CE = dyn_cast<CastExpr>(E);
          if (!E) {
            assert("unexpected cast failure");
            return CreateBoundsNone();
          }
          return RValueCastBounds(CE->getCastKind(), CE->getSubExpr());
        }
        // TODO: fill in these cases
        case Expr::UnaryOperatorClass:
        case Expr::BinaryOperatorClass:
        case Expr::CompoundAssignOperatorClass:
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

BoundsExpr *Sema::InferLValueBounds(ASTContext &Ctx, Expr *E) {
  return BoundsInference(Ctx).LValueBounds(E);
}

BoundsExpr *Sema::InferLValueTargetBounds(ASTContext &Ctx, Expr *E) {
  return BoundsInference(Ctx).LValueTargetBounds(E);
}

BoundsExpr *Sema::InferRValueBounds(ASTContext &Ctx, Expr *E) {
  return BoundsInference(Ctx).RValueBounds(E);
}

namespace {
  class CheckBoundsDeclarations :
    public RecursiveASTVisitor<CheckBoundsDeclarations> {
  private:
    Sema &S;
    bool DumpBounds;

    void DumpInferredBounds(raw_ostream &OS, BinaryOperator *E,
                            BoundsExpr *Target, BoundsExpr *B) {
      OS << "\nAssignment:\n";
      E->dump(OS);
      OS << "Target Bounds:\n";
      Target->dump(OS);
      OS << "RHS Bounds:\n ";
      B->dump(OS);;
    }

    void DumpInferredBounds(raw_ostream &OS, VarDecl *D,
                            BoundsExpr *Target, BoundsExpr *B) {
      OS << "\nDeclaration:\n";
      D->dump(OS);
      OS << "Declared Bounds:\n";
      Target->dump(OS);
      OS << "Initializer Bounds:\n ";
      B->dump(OS);;
    }

  public:
    CheckBoundsDeclarations(Sema &S) : S(S),
     DumpBounds(S.getLangOpts().DumpInferredBounds) {}

    bool VisitBinaryOperator(BinaryOperator *E) {
      Expr *LHS = E->getLHS();
      Expr *RHS = E->getRHS();
      QualType LHSType = LHS->getType();
      if (E->getOpcode() == BinaryOperatorKind::BO_Assign &&
          (LHSType->isCheckedPointerType() ||
           LHSType->isIntegralOrEnumerationType())) {
        BoundsExpr *LHSTargetBounds =
          S.InferLValueTargetBounds(S.getASTContext(), LHS);
        if (!LHSTargetBounds->isNone()) {
          BoundsExpr *RHSBounds = S.InferRValueBounds(S.getASTContext(), RHS);
          if (RHSBounds->isNone())
             S.Diag(LHS->getLocStart(), diag::err_expected_bounds);
          if (DumpBounds)
            DumpInferredBounds(llvm::outs(), E, LHSTargetBounds, RHSBounds);
        }
      }
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

      // Handle variables with bounds declarations
      BoundsExpr *DeclaredBounds = D->getBoundsExpr();
      if (!DeclaredBounds || DeclaredBounds->isInvalid() ||
          DeclaredBounds->isNone())
        return true;

      // If there is an initializer, check that the initializer meets the bounds
      // requirements for the variable.
      if (Expr *Init = D->getInit()) {
        assert(D->getInitStyle() == VarDecl::InitializationStyle::CInit);
        BoundsExpr *InitBounds = S.InferRValueBounds(S.getASTContext(), Init);
        if (InitBounds->isNone())
          S.Diag(Init->getLocStart(), diag::err_expected_bounds);
        if (DumpBounds)
          DumpInferredBounds(llvm::outs(), D, DeclaredBounds, InitBounds);
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

    bool VisitRangeBoundsExpr(const RangeBoundsExpr *E) {
      S.CheckIsNonModifyingExpr(E->getLowerExpr());
      S.CheckIsNonModifyingExpr(E->getUpperExpr());

      return true;
    }

    bool VisitCountBoundsExpr(const CountBoundsExpr *E) {
      S.CheckIsNonModifyingExpr(E->getCountExpr());
      
      return true;
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

  public:
    // Represents where the requirement that the checked expression is non-modifying
    // comes from.
    enum NonModifyingExprRequirement {
      NMER_Unknown,
      NMER_Dynamic_Check,
      NMER_Bounds_Count,
      NMER_Bounds_Byte_Count,
      NMER_Bounds_Range
    };

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
    NonModifiyingExprSema(Sema &S, NonModifyingExprRequirement From) : 
      S(S), FoundModifyingExpr(false), ReqFrom(From) {}

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
    NonModifyingExprRequirement ReqFrom;

    void addError(Expr *E, ModifyingExprKind Kind) {
      S.Diag(E->getLocStart(), diag::err_not_non_modifying_expr)
        << Kind << ReqFrom
        << E->getSourceRange();
    }
  };
}

bool Sema::CheckIsNonModifyingExpr(Expr *E) {
  auto requirement = NonModifiyingExprSema::NMER_Unknown;
  if (const CallExpr *Call = dyn_cast<CallExpr>(E)) {
    if (Call->getBuiltinCallee() == Builtin::BI_Dynamic_check) {
      requirement = NonModifiyingExprSema::NMER_Dynamic_Check;
    }
  }
  else if (isa<RangeBoundsExpr>(E)) {
    requirement = NonModifiyingExprSema::NMER_Bounds_Range;
  }
  else if (const CountBoundsExpr *Count = dyn_cast<CountBoundsExpr>(E)) {
    if (Count->getKind() == BoundsExpr::ElementCount) {
      requirement = NonModifiyingExprSema::NMER_Bounds_Count;
    }
    else if (Count->getKind() == BoundsExpr::ByteCount) {
      requirement = NonModifiyingExprSema::NMER_Bounds_Byte_Count;
    }
  }


  NonModifiyingExprSema Checker(*this, NonModifiyingExprSema::NMER_Unknown);
  Checker.TraverseStmt(E);

  return Checker.isNonModifyingExpr();
}
