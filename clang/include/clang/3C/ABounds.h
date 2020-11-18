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

#ifndef _ABOUNDS_H
#define _ABOUNDS_H

#include "ProgramVar.h"
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>

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
    // Bounds that represent number of bytes.
    ByteBoundKind,
    // Bounds that represent range.
    RangeBoundKind,
  };
  BoundsKind getKind() const { return Kind; }

private:
  BoundsKind Kind;

protected:
  ABounds(BoundsKind K) : Kind(K) {}
  void addBoundsUsedKey(BoundsKey);

public:
  virtual ~ABounds() {}

  virtual std::string mkString(AVarBoundsInfo *) = 0;
  virtual bool areSame(ABounds *, AVarBoundsInfo *) = 0;
  virtual BoundsKey getBKey() = 0;
  virtual ABounds *makeCopy(BoundsKey NK) = 0;

  // Set that maintains all the bound keys that are used inin
  // TODO: Is this still needed?
  static std::set<BoundsKey> KeysUsedInBounds;
  static bool isKeyUsedInBounds(BoundsKey ToCheck);

  static ABounds *getBoundsInfo(AVarBoundsInfo *AVBInfo, BoundsExpr *BExpr,
                                const ASTContext &C);
};

class CountBound : public ABounds {
public:
  CountBound(BoundsKey Var) : ABounds(CountBoundKind), CountVar(Var) {
    addBoundsUsedKey(Var);
  }

  virtual ~CountBound() {}

  std::string mkString(AVarBoundsInfo *ABI) override;
  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;
  BoundsKey getBKey() override;
  ABounds *makeCopy(BoundsKey NK) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == CountBoundKind;
  }

  BoundsKey getCountVar() { return CountVar; }

private:
  BoundsKey CountVar;
};

class ByteBound : public ABounds {
public:
  ByteBound(BoundsKey Var) : ABounds(ByteBoundKind), ByteVar(Var) {
    addBoundsUsedKey(Var);
  }

  virtual ~ByteBound() {}

  std::string mkString(AVarBoundsInfo *ABI) override;
  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;
  BoundsKey getBKey() override;
  ABounds *makeCopy(BoundsKey NK) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == ByteBoundKind;
  }
  BoundsKey getByteVar() { return ByteVar; }

private:
  BoundsKey ByteVar;
};

class RangeBound : public ABounds {
public:
  RangeBound(BoundsKey L, BoundsKey R) : ABounds(RangeBoundKind), LB(L), UB(R) {
    addBoundsUsedKey(L);
    addBoundsUsedKey(R);
  }

  virtual ~RangeBound() {}

  std::string mkString(AVarBoundsInfo *ABI) override;
  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;

  BoundsKey getBKey() override {
    assert(false && "Not implemented.");
    return 0;
  }

  ABounds *makeCopy(BoundsKey NK) override {
    assert(false && &"Not Implemented");
    return nullptr;
  }

  static bool classof(const ABounds *S) {
    return S->getKind() == RangeBoundKind;
  }

private:
  BoundsKey LB;
  BoundsKey UB;
};
#endif // _ABOUNDS_H
