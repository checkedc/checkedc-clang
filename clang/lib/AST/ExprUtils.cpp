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
                                              FieldDecl *Field, bool IsArrow) {
  ExprValueKind ResultKind;
  if (IsArrow)
    ResultKind = VK_LValue;
  else
    ResultKind = Base->isLValue() ? VK_LValue : VK_RValue;
  return MemberExpr::CreateImplicit(SemaRef.getASTContext(), Base, IsArrow,
                                    Field, Field->getType(), ResultKind,
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
