//=--ABounds.h----------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains bounds information of constraint variables.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_ABOUNDS_H
#define LLVM_CLANG_3C_ABOUNDS_H

#include "clang/3C/ProgramVar.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"

using namespace clang;

class AVarBoundsInfo;
// Class that represents bounds information of a program variable.
class ABounds {
public:
  enum BoundsKind {
    // Invalid bounds.
    InvalidKind,
    // Bounds that represent number of items.
    CountBoundKind,
    // Count bounds but plus one, i.e., count(i+1)
    CountPlusOneBoundKind,
    // Bounds that represent number of bytes.
    ByteBoundKind,
  };
  BoundsKind getKind() const { return Kind; }

protected:
  ABounds(BoundsKind K, BoundsKey L, BoundsKey B) : Kind(K), LengthKey(L),
                                                    LowerBoundKey(B) {}
  ABounds(BoundsKind K, BoundsKey L) : ABounds(K, L, 0) {}

  BoundsKind Kind;

  // Bounds key representing the length of the bounds from the base pointer of
  // the range. The exact interpretation of this field varies by subclass.
  BoundsKey LengthKey;

  // The base pointer representing the start of the range of the bounds. If this
  // is not equal to 0, then this ABounds has a specific lower bound that should
  // be used when emitting array pointer bounds. Otherwise, if it is 0, then the
  // lower bound should implicitly be the pointer the bound is applied to.
  BoundsKey LowerBoundKey;

  // Get the variable name of the the given bounds key that corresponds
  // to the given declaration.
  static std::string getBoundsKeyStr(BoundsKey, AVarBoundsInfo *,
                                     clang::Decl *);

public:
  virtual ~ABounds() {}

  // Make a string representation of this array bound. If the array has a
  // defined lower bound pointer that is not the same as the pointer for which
  // the bound string is being generated (passed as parameter BK), then a range
  // bound is generated using that lower bound. Otherwise, a standard count
  // bound is generated.
  std::string
  mkString(AVarBoundsInfo *ABI, clang::Decl *D = nullptr, BoundsKey BK = 0);

  // Make a string representation of this array bound always generating explicit
  // lower bounds in range bounds expressions.
  virtual std::string
  mkStringWithLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) = 0;

  // Make a string representation of this array bound ignoring any lower bound
  // information. A standard count bound is always generated.
  virtual std::string
  mkStringWithoutLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) = 0;

  virtual bool areSame(ABounds *, AVarBoundsInfo *) = 0;
  virtual ABounds *makeCopy(BoundsKey NK) = 0;

  BoundsKey getLengthKey() const { return LengthKey; }
  BoundsKey getLowerBoundKey() const { return LowerBoundKey; }
  void setLowerBoundKey(BoundsKey LB) { LowerBoundKey = LB; }

  static ABounds *getBoundsInfo(AVarBoundsInfo *AVBInfo, BoundsExpr *BExpr,
                                const ASTContext &C);
};

class CountBound : public ABounds {
public:
  CountBound(BoundsKey L, BoundsKey B) : ABounds(CountBoundKind, L, B) {}
  CountBound(BoundsKey L) : ABounds(CountBoundKind, L) {}

  std::string
  mkStringWithLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) override;
  std::string
  mkStringWithoutLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) override;

  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;

  ABounds *makeCopy(BoundsKey NK) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == CountBoundKind;
  }
};

class CountPlusOneBound : public CountBound {
public:
  CountPlusOneBound(BoundsKey L, BoundsKey B) : CountBound(L, B) {
    this->Kind = CountPlusOneBoundKind;
  }

  CountPlusOneBound(BoundsKey L) : CountBound(L) {
    this->Kind = CountPlusOneBoundKind;
  }

  std::string
  mkStringWithLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) override;
  std::string
  mkStringWithoutLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) override;

  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;
  ABounds *makeCopy(BoundsKey NK) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == CountPlusOneBoundKind;
  }
};

class ByteBound : public ABounds {
public:
  ByteBound(BoundsKey L, BoundsKey B) : ABounds(ByteBoundKind, L, B) {}
  ByteBound(BoundsKey L) : ABounds(ByteBoundKind, L) {}

  std::string
  mkStringWithLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) override;
  std::string
  mkStringWithoutLowerBound(AVarBoundsInfo *ABI, clang::Decl *D) override;

  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;
  ABounds *makeCopy(BoundsKey NK) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == ByteBoundKind;
  }
};

#endif // LLVM_CLANG_3C_ABOUNDS_H
