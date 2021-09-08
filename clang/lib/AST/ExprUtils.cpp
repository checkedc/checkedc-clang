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
  return ImplicitCastExpr::Create(SemaRef.Context, T, CK, E, nullptr,
                                  ExprValueKind::VK_RValue, FPOptionsOverride());
}

Expr *ExprCreatorUtil::CreateExplicitCast(Sema &SemaRef, QualType Target,
                                          CastKind CK, Expr *E,
                                          bool isBoundsSafeInterface) {
  // Avoid building up nested chains of no-op casts.
  E = ExprUtil::IgnoreRedundantCast(SemaRef.Context, CK, E);

  // Synthesize some dummy type source source information.
  TypeSourceInfo *DI = SemaRef.Context.getTrivialTypeSourceInfo(Target);
  CStyleCastExpr *CE = CStyleCastExpr::Create(SemaRef.Context, Target,
    ExprValueKind::VK_RValue, CK, E, nullptr, FPOptionsOverride(), DI,
    SourceLocation(), SourceLocation());
  CE->setBoundsSafeInterface(isBoundsSafeInterface);
  return CE;
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

UnaryOperator *ExprCreatorUtil::CreateUnaryOperator(Sema &SemaRef, Expr *Child,
                                                    UnaryOperatorKind Op) {
  return UnaryOperator::Create(SemaRef.Context, Child, Op,
                               Child->getType(),
                               Child->getValueKind(),
                               Child->getObjectKind(),
                               SourceLocation(),
                               /*CanOverflow*/ true,
                               FPOptionsOverride());
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

Expr *ExprUtil::IgnoreRedundantCast(ASTContext &Ctx, CastKind NewCK, Expr *E) {
  CastExpr *P = dyn_cast<CastExpr>(E);
  if (!P)
    return E;

  CastKind ExistingCK = P->getCastKind();
  Expr *SE = P->getSubExpr();
  if (NewCK == CK_BitCast && ExistingCK == CK_BitCast)
    return SE;

  return E;
}

bool ExprUtil::getReferentSizeInChars(ASTContext &Ctx, QualType Ty,
                                      llvm::APSInt &Size) {
  assert(Ty->isPointerType());
  const Type *Pointee = Ty->getPointeeOrArrayElementType();
  if (Pointee->isIncompleteType())
    return false;
  uint64_t ElemBitSize = Ctx.getTypeSize(Pointee);
  uint64_t ElemSize = Ctx.toCharUnitsFromBits(ElemBitSize).getQuantity();
  Size = llvm::APSInt(llvm::APInt(Ctx.getTargetInfo().getPointerWidth(0), ElemSize), false);
  return true;
}

llvm::APSInt ExprUtil::ConvertToSignedPointerWidth(ASTContext &Ctx,
                                                   llvm::APSInt I,
                                                   bool &Overflow) {
  uint64_t PointerWidth = Ctx.getTargetInfo().getPointerWidth(0);
  Overflow = false;
  if (I.getBitWidth() > PointerWidth) {
    Overflow = true;
    goto exit;
  }
  if (I.getBitWidth() < PointerWidth)
    I = I.extend(PointerWidth);
  if (I.isUnsigned()) {
    if (I > llvm::APSInt(I.getSignedMaxValue(PointerWidth))) {
      Overflow = true;
      goto exit;
    }
    I = llvm::APSInt(I, false);
  }
  exit:
    return I;
}

bool ExprUtil::EqualValue(ASTContext &Ctx, Expr *E1, Expr *E2,
                          EquivExprSets *EquivExprs) {
  Lexicographic::Result R = Lexicographic(Ctx, EquivExprs).CompareExpr(E1, E2);
  return R == Lexicographic::Result::Equal;
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

bool ExprUtil::IsDereferenceOrSubscript(Expr *E) {
  if (!E)
    return false;
  E = E->IgnoreParens();
  if (isa<ArraySubscriptExpr>(E))
    return true;
  UnaryOperator *UO = dyn_cast<UnaryOperator>(E);
  if (!UO)
    return false;
  return UO->getOpcode() == UnaryOperatorKind::UO_Deref;
}

bool ExprUtil::IsReturnValueExpr(Expr *E) {
  BoundsValueExpr *BVE = dyn_cast_or_null<BoundsValueExpr>(E);
  if (!BVE)
    return false;
  return BVE->getKind() == BoundsValueExpr::Kind::Return;
}

namespace {
  class FindLValueHelper : public RecursiveASTVisitor<FindLValueHelper> {
    private:
      Sema &SemaRef;
      Lexicographic Lex;
      Expr *LValue;
      bool Found;

    public:
      FindLValueHelper(Sema &SemaRef, Expr *LValue) :
        SemaRef(SemaRef),
        Lex(Lexicographic(SemaRef.Context, nullptr)),
        LValue(LValue),
        Found(false) {}

      bool IsFound() { return Found; }

      bool VisitDeclRefExpr(DeclRefExpr *E) {
        DeclRefExpr *V = dyn_cast_or_null<DeclRefExpr>(LValue);
        if (!V)
          return true;
        if (Lex.CompareExpr(V, E) == Lexicographic::Result::Equal)
          Found = true;
        return true;
      }

      bool VisitMemberExpr(MemberExpr *E) {
        MemberExpr *M = dyn_cast_or_null<MemberExpr>(LValue);
        if (!M)
          return true;
        if (Lex.CompareExprSemantically(E, M))
          Found = true;
        return true;
      }

      bool VisitUnaryOperator(UnaryOperator *E) {
        if (!ExprUtil::IsDereferenceOrSubscript(LValue))
          return true;
        if (Lex.CompareExprSemantically(E, LValue))
          Found = true;
        return true;
      }

      bool VisitArraySubscriptExpr(ArraySubscriptExpr *E) {
        if (!ExprUtil::IsDereferenceOrSubscript(LValue))
          return true;
        if (Lex.CompareExprSemantically(E, LValue))
          Found = true;
        return true;
      }

      // Do not traverse the child of a BoundsValueExpr.
      // Expressions within a BoundsValueExpr should not be considered
      // when looking for LValue.
      // For example, for the expression E = BoundsValue(TempBinding(LValue)),
      // FindLValue(LValue, E) should return false.
      bool TraverseBoundsValueExpr(BoundsValueExpr *E) {
        return true;
      }

      bool TraverseStmt(Stmt *S) {
        if (Found)
          return true;

        return RecursiveASTVisitor<FindLValueHelper>::TraverseStmt(S);
      }
  };
}

bool ExprUtil::FindLValue(Sema &S, Expr *LValue, Expr *E) {
  FindLValueHelper Finder(S, LValue);
  Finder.TraverseStmt(E);
  return Finder.IsFound();
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

      bool VisitUnaryOperator(UnaryOperator *E) {
        if (!ExprUtil::IsDereferenceOrSubscript(LValue))
          return true;
        if (Lex.CompareExprSemantically(E, LValue))
          ++Count;
        return true;
      }

      bool VisitArraySubscriptExpr(ArraySubscriptExpr *E) {
        if (!ExprUtil::IsDereferenceOrSubscript(LValue))
          return true;
        if (Lex.CompareExprSemantically(E, LValue))
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

void ExprUtil::EnsureEqualBitWidths(llvm::APSInt &A, llvm::APSInt &B) {
  if (A.getBitWidth() < B.getBitWidth())
    A = A.extOrTrunc(B.getBitWidth());
  else if (B.getBitWidth() < A.getBitWidth())
    B = B.extOrTrunc(A.getBitWidth());
}

bool InverseUtil::IsInvertible(Sema &S, Expr *LValue, Expr *E) {
  if (!E || E->containsErrors())
    return false;

  E = E->IgnoreParens();
  Expr *RValueChild = ExprUtil::GetRValueCastChild(S, E);
  if (RValueChild && ExprUtil::EqualValue(S.Context, LValue, RValueChild, nullptr))
    return true;

  switch (E->getStmtClass()) {
    case Expr::UnaryOperatorClass:
      return IsUnaryOperatorInvertible(S, LValue, cast<UnaryOperator>(E));
    case Expr::BinaryOperatorClass:
      return IsBinaryOperatorInvertible(S, LValue, cast<BinaryOperator>(E));
    case Expr::ImplicitCastExprClass:
    case Expr::CStyleCastExprClass:
    case Expr::BoundsCastExprClass:
      return IsCastExprInvertible(S, LValue, cast<CastExpr>(E));
    default:
      return false;
  }
}

bool InverseUtil::IsUnaryOperatorInvertible(Sema &S, Expr *LValue,
                                            UnaryOperator *E) {
  Expr *SubExpr = E->getSubExpr()->IgnoreParens();
  UnaryOperatorKind Op = E->getOpcode();

  if (Op == UnaryOperatorKind::UO_AddrOf) {
    // &*e1 is invertible with respect to LValue if e1 is invertible with
    // respect to LValue.
    if (UnaryOperator *UnarySubExpr = dyn_cast<UnaryOperator>(SubExpr)) {
      if (UnarySubExpr->getOpcode() == UnaryOperatorKind::UO_Deref)
        return IsInvertible(S, LValue, UnarySubExpr->getSubExpr());
    }
    // &e1[e2] is invertible with respect to LValue if e1 + e2 is invertible
    // with respect to LValue.
    else if (ArraySubscriptExpr *ArraySubExpr = dyn_cast<ArraySubscriptExpr>(SubExpr)) {
      Expr *Base = ArraySubExpr->getBase();
      Expr *Index = ArraySubExpr->getIdx();
      BinaryOperator Sum(S.Context, Base, Index,
                         BinaryOperatorKind::BO_Add,
                         Base->getType(),
                         Base->getValueKind(),
                         Base->getObjectKind(),
                         SourceLocation(),
                         FPOptionsOverride());
      return IsInvertible(S, LValue, &Sum);
    }
  }

  // *&e1 is invertible with respect to LValue if e1 is invertible with
  // respect to LValue.
  if (Op == UnaryOperatorKind::UO_Deref) {
    if (UnaryOperator *UnarySubExpr = dyn_cast<UnaryOperator>(SubExpr)) {
      if (UnarySubExpr->getOpcode() == UnaryOperatorKind::UO_AddrOf)
        return IsInvertible(S, LValue, UnarySubExpr->getSubExpr());
    }
  }

  // ~e1, -e1, and +e1 are invertible with respect to LValue if e1 is
  // invertible with respect to LValue.
  if (Op == UnaryOperatorKind::UO_Not ||
      Op == UnaryOperatorKind::UO_Minus ||
      Op == UnaryOperatorKind::UO_Plus)
    return IsInvertible(S, LValue, SubExpr);

  return false;
}

bool InverseUtil::IsBinaryOperatorInvertible(Sema &S, Expr *LValue,
                                          BinaryOperator *E) {
  BinaryOperatorKind Op = E->getOpcode();
  if (Op != BinaryOperatorKind::BO_Add &&
      Op != BinaryOperatorKind::BO_Sub &&
      Op != BinaryOperatorKind::BO_Xor)
    return false;

  Expr *LHS = E->getLHS();
  Expr *RHS = E->getRHS();

  // Addition and subtraction operations must be for pointer arithmetic
  // or unsigned integer arithmetic.
  if (Op == BinaryOperatorKind::BO_Add || Op == BinaryOperatorKind::BO_Sub) {
    // The operation is pointer arithmetic if either the LHS or the RHS
    // have pointer type.
    bool IsPtrArithmetic = LHS->getType()->isPointerType() ||
                           RHS->getType()->isPointerType();
    if (!IsPtrArithmetic) {
      // The operation is unsigned integer arithmetic if both the LHS
      // and the RHS have unsigned integer type.
      bool IsUnsignedArithmetic = LHS->getType()->isUnsignedIntegerType() &&
                                  RHS->getType()->isUnsignedIntegerType();
      if (!IsUnsignedArithmetic)
        return false;
    }
  }

  // LValue must appear in exactly one subexpression of E and that
  // subexpression must be invertible with respect to LValue.
  std::pair<Expr *, Expr*> Pair =
    ExprUtil::SplitByLValueCount(S, LValue, LHS, RHS);
  if (!Pair.first)
    return false;
  Expr *E_LValue = Pair.first, *E_NotLValue = Pair.second;
  if (!IsInvertible(S, LValue, E_LValue))
    return false;

  // The subexpression not containing LValue must be nonmodifying
  // and cannot be or contain a pointer dereference, member
  // reference, or indirect member reference.
  if (!ExprUtil::CheckIsNonModifying(S, E_NotLValue) ||
      ExprUtil::ReadsMemoryViaPointer(E_NotLValue, true))
    return false;

  return true;
}

bool InverseUtil::IsCastExprInvertible(Sema &S, Expr *LValue, CastExpr *E) {
  QualType T1 = E->getType();
  QualType T2 = E->getSubExpr()->getType();
  uint64_t Size1 = S.Context.getTypeSize(T1);
  uint64_t Size2 = S.Context.getTypeSize(T2);

  // If T1 is a smaller type than T2, then (T1)e1 is a narrowing cast.
  if (Size1 < Size2)
    return false;

  switch (E->getCastKind()) {
    // Bit-preserving casts
    case CastKind::CK_BitCast:
    case CastKind::CK_LValueBitCast:
    case CastKind::CK_NoOp:
    case CastKind::CK_ArrayToPointerDecay:
    case CastKind::CK_FunctionToPointerDecay:
    case CastKind::CK_NullToPointer:
    // Widening casts
    case CastKind::CK_BooleanToSignedIntegral:
    case CastKind::CK_IntegralToFloating:
      return IsInvertible(S, LValue, E->getSubExpr());
    // Bounds casts may be invertible.
    case CastKind::CK_DynamicPtrBounds:
    case CastKind::CK_AssumePtrBounds: {
      CHKCBindTemporaryExpr *Temp =
        dyn_cast<CHKCBindTemporaryExpr>(E->getSubExpr());
      assert(Temp);
      return IsInvertible(S, LValue, Temp->getSubExpr());
    }
    // Potentially non-narrowing casts, depending on type sizes
    case CastKind::CK_IntegralToPointer:
    case CastKind::CK_PointerToIntegral:
    case CastKind::CK_IntegralCast:
      return Size1 >= Size2 && IsInvertible(S, LValue, E->getSubExpr());
    // All other casts are considered narrowing.
    default:
      return false;
  }
}

Expr *InverseUtil::Inverse(Sema &S, Expr *LValue, Expr *F, Expr *E) {
  if (!F || F->containsErrors())
    return nullptr;

  if (!E || E->containsErrors())
    return nullptr;

  E = E->IgnoreParens();
  Expr *RValueChild = ExprUtil::GetRValueCastChild(S, E);
  if (RValueChild && ExprUtil::EqualValue(S.Context, LValue, RValueChild, nullptr))
    return F;

  switch (E->getStmtClass()) {
    case Expr::UnaryOperatorClass:
      return UnaryOperatorInverse(S, LValue, F, cast<UnaryOperator>(E));
    case Expr::BinaryOperatorClass:
      return BinaryOperatorInverse(S, LValue, F, cast<BinaryOperator>(E));
    case Expr::ImplicitCastExprClass:
    case Expr::CStyleCastExprClass:
    case Expr::BoundsCastExprClass:
      return CastExprInverse(S, LValue, F, cast<CastExpr>(E));
    default:
      return nullptr;
  }

  return nullptr;
}

Expr *InverseUtil::UnaryOperatorInverse(Sema &S, Expr *LValue, Expr *F,
                                        UnaryOperator *E) {
  Expr *SubExpr = E->getSubExpr()->IgnoreParens();
  UnaryOperatorKind Op = E->getOpcode();
  
  if (Op == UnaryOperatorKind::UO_AddrOf) {
    // Inverse(f, &*e1) = Inverse(f, e1)
    if (UnaryOperator *UnarySubExpr = dyn_cast<UnaryOperator>(SubExpr)) {
      if (UnarySubExpr->getOpcode() == UnaryOperatorKind::UO_Deref)
        return Inverse(S, LValue, F, UnarySubExpr->getSubExpr());
    }
    // Inverse(f, &e1[e2]) = Inverse(f, e1 + e2)
    else if (ArraySubscriptExpr *ArraySubExpr = dyn_cast<ArraySubscriptExpr>(SubExpr)) {
      Expr *Base = ArraySubExpr->getBase();
      Expr *Index = ArraySubExpr->getIdx();
      BinaryOperator Sum(S.Context, Base, Index,
                         BinaryOperatorKind::BO_Add,
                         Base->getType(),
                         Base->getValueKind(),
                         Base->getObjectKind(),
                         SourceLocation(),
                         FPOptionsOverride());
      return Inverse(S, LValue, F, &Sum);
    }
  }

  // Inverse(f, *&e1) = Inverse(f, e1)
  if (Op == UnaryOperatorKind::UO_Deref) {
    if (UnaryOperator *UnarySubExpr = dyn_cast<UnaryOperator>(SubExpr)) {
      if (UnarySubExpr->getOpcode() == UnaryOperatorKind::UO_AddrOf)
        return Inverse(S, LValue, F, UnarySubExpr->getSubExpr());
    }
  }

  // Inverse(f, ~e1) = Inverse(~f, e1)
  // Inverse(f, -e1) = Inverse(-f, e1)
  // Inverse(f, +e1) = Inverse(+f, e1)
  Expr *Child = ExprCreatorUtil::EnsureRValue(S, F);
  Expr *F1 = UnaryOperator::Create(S.Context, Child, Op,
                                   E->getType(),
                                   E->getValueKind(),
                                   E->getObjectKind(),
                                   SourceLocation(),
                                   E->canOverflow(),
                                   FPOptionsOverride());
  return Inverse(S, LValue, F1, SubExpr);
}

Expr *InverseUtil::BinaryOperatorInverse(Sema &S, Expr *LValue, Expr *F,
                                         BinaryOperator *E) {
  std::pair<Expr *, Expr*> Pair =
    ExprUtil::SplitByLValueCount(S, LValue, E->getLHS(), E->getRHS());
  if (!Pair.first)
    return nullptr;

  Expr *E_LValue = Pair.first, *E_NotLValue = Pair.second;
  BinaryOperatorKind Op = E->getOpcode();
  Expr *F1 = nullptr;

  switch (Op) {
    case BinaryOperatorKind::BO_Add:
      // Inverse(f, e1 + e2) = Inverse(f - e_notlvalue, e_lvalue)
      F1 = ExprCreatorUtil::CreateBinaryOperator(S, F, E_NotLValue, BinaryOperatorKind::BO_Sub);
      break;
    case BinaryOperatorKind::BO_Sub: {
      if (E_LValue == E->getLHS())
        // Inverse(f, e_lvalue - e_notlvalue) = Inverse(f + e_notlvalue, e_lvalue)
        F1 = ExprCreatorUtil::CreateBinaryOperator(S, F, E_NotLValue, BinaryOperatorKind::BO_Add);
      else
        // Inverse(f, e_notlvalue - e_lvalue) => Inverse(e_notlvalue - f, e_lvalue)
        F1 = ExprCreatorUtil::CreateBinaryOperator(S, E_NotLValue, F, BinaryOperatorKind::BO_Sub);
      break;
    }
    case BinaryOperatorKind::BO_Xor:
      // Inverse(f, e1 ^ e2) = Inverse(lvalue, f ^ e_notlvalue, e_lvalue)
      F1 = ExprCreatorUtil::CreateBinaryOperator(S, F, E_NotLValue, BinaryOperatorKind::BO_Xor);
      break;
    default:
      llvm_unreachable("unexpected binary operator kind");
  }

  return Inverse(S, LValue, F1, E_LValue);
}

Expr *InverseUtil::CastExprInverse(Sema &S, Expr *LValue, Expr *F, CastExpr *E) {
  QualType T2 = E->getSubExpr()->getType();
  switch (E->getStmtClass()) {
    case Expr::ImplicitCastExprClass: {
      Expr *F1 =
        ExprCreatorUtil::CreateImplicitCast(S, F, E->getCastKind(), T2);
      return Inverse(S, LValue, F1, E->getSubExpr());
    }
    case Expr::CStyleCastExprClass: {
      Expr *F1 =
        ExprCreatorUtil::CreateExplicitCast(S, T2, E->getCastKind(), F,
                                            E->isBoundsSafeInterface());
      return Inverse(S, LValue, F1, E->getSubExpr());
    }
    case Expr::BoundsCastExprClass: {
      CHKCBindTemporaryExpr *Temp = dyn_cast<CHKCBindTemporaryExpr>(E->getSubExpr());
      assert(Temp);
      Expr *F1 =
        ExprCreatorUtil::CreateExplicitCast(S, T2, CastKind::CK_BitCast, F,
                                            E->isBoundsSafeInterface());
      return Inverse(S, LValue, F1, Temp->getSubExpr());
    }
    default:
      llvm_unreachable("unexpected cast kind");
  }
  return nullptr;
}
