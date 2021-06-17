//===--------- ExprUtils.cpp: Utility functions for expressions ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements utility functions for expressions.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ExprUtils.h"
#include "clang/AST/RecursiveASTVisitor.h"

using namespace clang;

BinaryOperator *ExprCreatorUtil::CreateBinaryOperator(Sema &SemaRef,
                                                      Expr *LHS, Expr *RHS,
                                                      BinaryOperatorKind Op) {
  assert(LHS && "expected LHS to exist");
  assert(RHS && "expected RHS to exist");
  LHS = EnsureRValue(SemaRef, LHS);
  RHS = EnsureRValue(SemaRef, RHS);
  if (BinaryOperator::isCompoundAssignmentOp(Op))
    Op = BinaryOperator::getOpForCompoundAssignment(Op);
  return BinaryOperator::Create(SemaRef.Context, LHS, RHS, Op,
                                LHS->getType(), LHS->getValueKind(),
                                LHS->getObjectKind(), SourceLocation(),
                                FPOptionsOverride());
}

IntegerLiteral *ExprCreatorUtil::CreateUnsignedInt(Sema &SemaRef,
                                                   unsigned Value) {
  QualType T = SemaRef.Context.UnsignedIntTy;
  llvm::APInt Val(SemaRef.Context.getIntWidth(T), Value);
  return IntegerLiteral::Create(SemaRef.Context, Val,
                                T, SourceLocation());
}

ImplicitCastExpr *ExprCreatorUtil::CreateImplicitCast(Sema &SemaRef, Expr *E,
                                                      CastKind CK,
                                                      QualType T) {
  return ImplicitCastExpr::Create(SemaRef.Context, T,
                                  CK, E, nullptr,
                                  ExprValueKind::VK_RValue);
}

DeclRefExpr *ExprCreatorUtil::CreateVarUse(Sema &SemaRef, VarDecl *V) {
  return DeclRefExpr::Create(SemaRef.getASTContext(), NestedNameSpecifierLoc(),
                             SourceLocation(), V, false, SourceLocation(),
                             V->getType(), ExprValueKind::VK_LValue);
}

MemberExpr *ExprCreatorUtil::CreateMemberExpr(Sema &SemaRef, Expr *Base,
                                              const FieldDecl *Field, bool IsArrow) {
  ExprValueKind ResultKind;
  if (IsArrow)
    ResultKind = VK_LValue;
  else
    ResultKind = Base->isLValue() ? VK_LValue : VK_RValue;
  FieldDecl *F = const_cast<FieldDecl *>(Field);
  return MemberExpr::CreateImplicit(SemaRef.getASTContext(), Base, IsArrow,
                                    F, F->getType(), ResultKind,
                                    OK_Ordinary);
}

Expr *ExprCreatorUtil::EnsureRValue(Sema &SemaRef, Expr *E) {
  if (E->isRValue())
    return E;

  CastKind Kind;
  QualType TargetTy;
  if (E->getType()->isArrayType()) {
    Kind = CK_ArrayToPointerDecay;
    TargetTy = SemaRef.getASTContext().getArrayDecayedType(E->getType());
  } else {
    Kind = CK_LValueToRValue;
    TargetTy = E->getType();
  }
  return CreateImplicitCast(SemaRef, E, Kind, TargetTy);
}

IntegerLiteral *ExprCreatorUtil::CreateIntegerLiteral(ASTContext &Ctx,
                                                      const llvm::APInt &I) {
  QualType Ty;
  // Choose the type of an integer constant following the rules in
  // Section 6.4.4 of the C11 specification: the smallest integer
  // type chosen from int, long int, long long int, unsigned long long
  // in which the integer fits.
  llvm::APInt ResultVal;
  if (Fits(Ctx, Ctx.IntTy, I, ResultVal))
    Ty = Ctx.IntTy;
  else if (Fits(Ctx, Ctx.LongTy, I, ResultVal))
    Ty = Ctx.LongTy;
  else if (Fits(Ctx, Ctx.LongLongTy, I, ResultVal))
    Ty = Ctx.LongLongTy;
  else {
    assert(I.getBitWidth() <=
           Ctx.getIntWidth(Ctx.UnsignedLongLongTy));
    ResultVal = I;
    Ty = Ctx.UnsignedLongLongTy;
  }
  IntegerLiteral *Lit = IntegerLiteral::Create(Ctx, ResultVal, Ty,
                                               SourceLocation());
  return Lit;
}

IntegerLiteral *ExprCreatorUtil::CreateIntegerLiteral(ASTContext &Ctx,
                                                      int Value, QualType Ty) {
  if (Ty->isPointerType()) {
    const llvm::APInt
      ResultVal(Ctx.getTargetInfo().getPointerWidth(0), Value);
    return CreateIntegerLiteral(Ctx, ResultVal);
  }

  if (!Ty->isIntegerType())
    return nullptr;

  unsigned BitSize = Ctx.getTypeSize(Ty);
  unsigned IntWidth = Ctx.getIntWidth(Ty);
  if (BitSize != IntWidth)
    return nullptr;

  const llvm::APInt ResultVal(BitSize, Value);
  return IntegerLiteral::Create(Ctx, ResultVal, Ty, SourceLocation());
}

bool ExprCreatorUtil::Fits(ASTContext &Ctx, QualType Ty,
                           const llvm::APInt &I, llvm::APInt &Result) {
  assert(Ty->isSignedIntegerType());
  unsigned bitSize = Ctx.getTypeSize(Ty);
  if (bitSize < I.getBitWidth()) {
    if (bitSize < I.getActiveBits())
     // Number of bits in use exceeds bitsize
     return false;
    else Result = I.trunc(bitSize);
  } else if (bitSize > I.getBitWidth())
    Result = I.zext(bitSize);
  else
    Result = I;
  return Result.isNonNegative();
}

DeclRefExpr *VariableUtil::GetLValueVariable(Sema &S, Expr *E) {
  Lexicographic Lex(S.Context, nullptr);
  E = Lex.IgnoreValuePreservingOperations(S.Context, E);
  return dyn_cast<DeclRefExpr>(E);
}

DeclRefExpr *VariableUtil::GetRValueVariable(Sema &S, Expr *E) {
  if (!E)
    return nullptr;
  if (CastExpr *CE = dyn_cast<CastExpr>(E->IgnoreParens())) {
    CastKind CK = CE->getCastKind();
    if (CK == CastKind::CK_LValueToRValue ||
        CK == CastKind::CK_ArrayToPointerDecay)
      return GetLValueVariable(S, CE->getSubExpr());
  }
  return nullptr;
}

bool VariableUtil::IsRValueCastOfVariable(Sema &S, Expr *E, DeclRefExpr *V) {
  DeclRefExpr *Var = GetRValueVariable(S, E);
  if (!Var)
    return false;
  Lexicographic Lex(S.Context, nullptr);
  return Lex.CompareExpr(V, Var) == Lexicographic::Result::Equal;
}

Expr *ExprUtil::GetRValueCastChild(Sema &S, Expr *E) {
  if (!E)
    return nullptr;
  E = E->IgnoreParens();
  if (CastExpr *CE = dyn_cast<CastExpr>(E)) {
    CastKind CK = CE->getCastKind();
    if (CK == CastKind::CK_LValueToRValue ||
        CK == CastKind::CK_ArrayToPointerDecay)
      return CE->getSubExpr()->IgnoreParens();
  }
  return nullptr;
}

bool ExprUtil::CheckIsNonModifying(Sema &S, Expr *E) {
  return S.CheckIsNonModifying(E, Sema::NonModifyingContext::NMC_Unknown,
                               Sema::NonModifyingMessage::NMM_None);
}

bool ExprUtil::ReadsMemoryViaPointer(Expr *E, bool IncludeAllMemberExprs) {
  if (!E)
    return false;

  E = E->IgnoreParens();

  switch (E->getStmtClass()) {
    case Expr::UnaryOperatorClass: {
      UnaryOperator *UO = cast<UnaryOperator>(E);
      // *e reads memory via a pointer.
      return UO->getOpcode() == UnaryOperatorKind::UO_Deref;
    }
    // e1[e2] is a synonym for *(e1 + e2), which reads memory via a pointer.
    case Expr::ArraySubscriptExprClass:
      return true;
    case Expr::MemberExprClass: {
      if (IncludeAllMemberExprs)
        return true;

      MemberExpr *ME = cast<MemberExpr>(E);
      // e1->f reads memory via a pointer.
      if (ME->isArrow())
        return true;
      // e1.f reads memory via a pointer if and only if e1 reads
      // memory via a pointer.
      else
        return ReadsMemoryViaPointer(ME->getBase(), IncludeAllMemberExprs);
    }
    default: {
      for (auto I = E->child_begin(); I != E->child_end(); ++I) {
        if (Expr *SubExpr = dyn_cast<Expr>(*I)) {
          if (ReadsMemoryViaPointer(SubExpr, IncludeAllMemberExprs))
            return true;
        }
      }
      return false;
    }
  }
}

std::pair<Expr *, Expr *> ExprUtil::SplitByLValueCount(Sema &S, Expr *LValue,
                                                       Expr *E1, Expr *E2) {
  std::pair<Expr *, Expr *> Pair;
  unsigned int Count1 = LValueOccurrenceCount(S, LValue, E1);
  unsigned int Count2 = LValueOccurrenceCount(S, LValue, E2);
  if (Count1 == 1 && Count2 == 0) {
    // LValue appears once in E1 and does not appear in E2.
    Pair.first = E1;
    Pair.second = E2;
  } else if (Count2 == 1 && Count1 == 0) {
    // LValue appears once in E2 and does not appear in E1.
    Pair.first = E2;
    Pair.second = E1;
  }
  return Pair;
}

namespace {
  class LValueCountHelper : public RecursiveASTVisitor<LValueCountHelper> {
    private:
      Sema &SemaRef;
      Lexicographic Lex;
      Expr *LValue;
      ValueDecl *V;
      unsigned int Count;

    public:
      LValueCountHelper(Sema &SemaRef, Expr *LValue, ValueDecl *V) :
        SemaRef(SemaRef),
        Lex(Lexicographic(SemaRef.Context, nullptr)),
        LValue(LValue),
        V(V),
        Count(0) {}

      unsigned int GetCount() { return Count; }

      bool VisitDeclRefExpr(DeclRefExpr *E) {
        // Check for an occurrence of a variable whose declaration matches V.
        if (V) {
          if (ValueDecl *D = E->getDecl()) {
            if (Lex.CompareDecl(D, V) == Lexicographic::Result::Equal)
              ++Count;
          }
          return true;
        }

        // Check for an occurrence of a variable equal to LValue if LValue
        // is a variable.
        DeclRefExpr *Var = dyn_cast_or_null<DeclRefExpr>(LValue);
        if (!Var)
          return true;
        if (Lex.CompareExpr(Var, E) == Lexicographic::Result::Equal)
          ++Count;
        return true;
      }

      bool VisitMemberExpr(MemberExpr *E) {
        MemberExpr *M = dyn_cast_or_null<MemberExpr>(LValue);
        if (!M)
          return true;
        if (Lex.CompareExprSemantically(E, M))
          ++Count;
        return true;
      }

      // Do not traverse the child of a BoundsValueExpr.
      // If a BoundsValueExpr uses the expression LValue (or a variable whose
      // declaration matches V), this should not count toward the total
      // occurrence count of LValue or V in the expression.
      // For example, for the expression BoundsValue(TempBinding(v)) + v, the
      // total occurrence count of the variable v should be 1, not 2.
      bool TraverseBoundsValueExpr(BoundsValueExpr *E) {
        return true;
      }
  };
}

unsigned int ExprUtil::LValueOccurrenceCount(Sema &S, Expr *LValue, Expr *E) {
  LValueCountHelper Counter(S, LValue, nullptr);
  Counter.TraverseStmt(E);
  return Counter.GetCount();
}

unsigned int ExprUtil::VariableOccurrenceCount(Sema &S, ValueDecl *V, Expr *E) {
  if (!V)
    return 0;
  LValueCountHelper Counter(S, nullptr, V);
  Counter.TraverseStmt(E);
  return Counter.GetCount();
}

unsigned int ExprUtil::VariableOccurrenceCount(Sema &S, DeclRefExpr *Target,
                                               Expr *E) {
  return VariableOccurrenceCount(S, Target->getDecl(), E);
}
