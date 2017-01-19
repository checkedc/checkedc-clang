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
  // Class for inferring bounds expressions for C expressions.

  // C has an interesting semantics for expressions that differentiates between
  // lvalue and value expressions and inserts implicit conversions from lvalues
  // to values.  Value expressions are usually called rvalue expression.  This
  // seantics is represented directly in the clang IR by having some
  // expressions evaluate to lvalues and having implict conversions that convert
  // those lvalues to rvalues.
  //
  // Using ths representation directly would make it clumsy to compute bounds
  // expressions.  For an expression that evaluates to an lvalue, we would have
  // to compute and carry along two bounds expressions: the bounds expression
  // for the lvalue and the bounds expression for the value at which the lvalue
  // points.
  //
  // We take a slightly different approach for computing bounds.  We say that
  // depending on the context where an expression occurs, expressions in C may
  // denote either values or location sof objects in memory (lvalues).  We then
  // have two methods for determining the bounds expression of an expression:
  // one for an expression that denotes an rvalue and another for an expression
  // that denotes an lvalue.  The method to invoke depends on the context in
  // which an expression occurs.
  //
  // An expression denotes an lvalue if it occurs in the following contexts:
  // 1. As the left-hand side of an assignment operator.
  // 2. As the operand to a postfix or prefix incrementation operators (which
  //    implicitly do assignment).
  // 3. As the operand of the address-of (&) operator.
  // 4. If a member access operation e1.f denotes on lvalue, e1 denotes an lvalue.
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

    BoundsExpr *RValueIntegerLiteral(IntegerLiteral *L) {
      return CreateBoundsAny();
    }

  public:
    BoundsInference(ASTContext &Ctx) : Context(Ctx) {
    }

    BoundsExpr *LValueBounds(Expr *E) {
      E = E->IgnoreParens();
      switch (E->getStmtClass()) {
      case Expr::DeclRefExprClass:
      case Expr::UnaryOperatorClass:
      case Expr::ArraySubscriptExprClass:
      case Expr::MemberExprClass:
        return CreateBoundsAny();
      default:
        return CreateBoundsNone();
      }
    }

    BoundsExpr *RValueBounds(Expr *E) {
      E = E->IgnoreParens();
      switch (E->getStmtClass()) {
        case Expr::IntegerLiteralClass: {
         if (IntegerLiteral *Lit = dyn_cast<IntegerLiteral>(E))
            return RValueIntegerLiteral(Lit);
         llvm_unreachable("unexpected cast failure");
        }
        case Expr::DeclRefExprClass:
        case Expr::UnaryOperatorClass:
        case Expr::ArraySubscriptExprClass:
        case Expr::BinaryOperatorClass:
        case Expr::CompoundAssignOperatorClass:
        case Expr::MemberExprClass:
        case Expr::ImplicitCastExprClass:
        case Expr::CStyleCastExprClass:
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

BoundsExpr *Sema::InferRValueBounds(ASTContext &Ctx, Expr *E) {
  return BoundsInference(Ctx).RValueBounds(E);
}

namespace {
  class CheckBoundsDeclarations : public RecursiveASTVisitor<CheckBoundsDeclarations> {
  private:
    Sema &S;

  public:
    CheckBoundsDeclarations(Sema &S) : S(S) {}

    bool VisitBinaryOperator(BinaryOperator *E) {
      Expr *LHS = E->getLHS();
      Expr *RHS = E->getRHS();
      QualType LHSType = LHS->getType();
      if (E->getOpcode() == BinaryOperatorKind::BO_Assign &&
          (LHSType->isCheckedPointerType() ||
           LHSType->isIntegralOrEnumerationType())) {
        BoundsExpr *LHSBounds = S.InferRValueBounds(S.getASTContext(), LHS);
        if (!LHSBounds->isNone()) {
          BoundsExpr *RHSBounds = S.InferRValueBounds(S.getASTContext(), RHS);
          if (RHSBounds->isNone())
             S.Diag(LHS->getLocStart(), diag::err_expected_bounds);
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
  };
}

void Sema::CheckFunctionBodyBoundsDecls(FunctionDecl *FD, Stmt *Body) {
  CheckBoundsDeclarations(*this).TraverseStmt(Body);
}

void Sema::CheckTopLevelBoundsDecls(VarDecl *D) {
  if (!D->isLocalVarDeclOrParm())
    CheckBoundsDeclarations(*this).TraverseVarDecl(D);
}
