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
  class InferBoundsExpr {

  private:
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
    InferBoundsExpr(ASTContext &Ctx) : Context(Ctx) {
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
          IntegerLiteral *Lit = dyn_cast<IntegerLiteral>(E);
          if (Lit)
            return RValueIntegerLiteral(Lit);
          else
            llvm_unreachable("unexpected cast failure");
          break;
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
  return InferBoundsExpr(Ctx).LValueBounds(E);
}

BoundsExpr *Sema::InferRValueBounds(ASTContext &Ctx, Expr *E) {
  return InferBoundsExpr(Ctx).RValueBounds(E);
}

namespace {
  class CheckFunctionBodyBounds : public RecursiveASTVisitor<CheckFunctionBodyBounds> {
  private:
    Sema &S;

  public:
    CheckFunctionBodyBounds(Sema &S) : S(S) {}

    bool VisitBinaryOperator(BinaryOperator *E) {
      Expr *LHS = E->getLHS();
      Expr *RHS = E->getRHS();
      if (E->getOpcode() == BinaryOperatorKind::BO_Assign &&
          LHS->getType()->isCheckedPointerType()) {
        BoundsExpr *LHSBounds = S.InferRValueBounds(S.getASTContext(), LHS);
        if (!LHSBounds->isNone()) {
          BoundsExpr *RHSBounds = S.InferRValueBounds(S.getASTContext(), RHS);
          if (RHSBounds->isNone())
             S.Diag(LHS->getLocStart(), diag::err_expected_bounds);
        }
      }
      return true;
    }
  };
}


void Sema::CheckCheckedCFunctionBody(FunctionDecl *FD, Stmt *Body) {
  CheckFunctionBodyBounds(*this).TraverseStmt(Body);
}

