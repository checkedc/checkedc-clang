//===--- CheckedCAnalysesPrepass.h: Data used by Checked C analyses ---===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//
//
//  This file defines a set of information that is gathered in a single
//  pass over a function. This information is used by different Checked C
//  analyses such as bounds declaration checking, bounds widening, etc.
//
//===------------------------------------------------------------------===//

#ifndef LLVM_CLANG_CHECKEDC_ANALYSES_PREPASS_H
#define LLVM_CLANG_CHECKEDC_ANALYSES_PREPASS_H

#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"

namespace clang {
  // VarUsageTy maps a VarDecl to a DeclRefExpr that is a use of the VarDecl.
  using VarUsageTy = llvm::DenseMap<const VarDecl *, DeclRefExpr *>;

  // VarSetTy denotes a set of variables.
  using VarSetTy = llvm::SmallPtrSet<const VarDecl *, 2>;

  // BoundsVarsTy maps a variable Z to the set of all variables in whose bounds
  // expressions Z occurs.
  using BoundsVarsTy = llvm::DenseMap<const VarDecl *, VarSetTy>;

  // FieldSetTy denotes a set of fields.
  using FieldSetTy = llvm::SmallPtrSet<const FieldDecl *, 2>;

  // BoundsSiblingFieldsTy maps a field F to the set of all sibling fields
  // of F in whose declared bounds expressions F occurs.
  using BoundsSiblingFieldsTy = llvm::DenseMap<const FieldDecl *, FieldSetTy>;

  struct PrepassInfo {
    // VarUses maps each VarDecl V in a function to the DeclRefExpr (if any)
    // that is the first use of V, if V fulfills the following conditions:
    // 1. V is used in a declared bounds expression, or:
    // 2. V has a declared bounds expression.
    VarUsageTy VarUses;

    // BoundsVars maps each variable Z in a function to the set of all
    // variables in whose bounds expressions Z occurs. A variable Z can occur
    // in the bounds expression of a variable V if
    // 1. Z occurs in the declared bounds expression of V, or
    // 2. A where clause declares bounds B of V and Z occurs in B.

    // Note: BoundsVarsTy is a map of keys to values which are sets. As a
    // result, there is no defined iteration order for either its keys or its
    // values. So in case we want to iterate BoundsVars and need a determinstic
    // iteration order we must remember to sort the keys as well as the values.
    BoundsVarsTy BoundsVars;

    // BoundsSiblingFields maps each FieldDecl F in a struct declaration S to
    // a set of fields in S in whose declared bounds F occurs.

    // Note: BoundsSiblingFieldsTy is a map of keys to values which are sets.
    // As a result, there is no defined iteration order for either its keys or
    // its values. So in case we want to iterate BoundsSiblingFields and need
    // a deterministic iteration order we must remember to sort the keys as
    // well as the values.
    BoundsSiblingFieldsTy BoundsSiblingFields;
  };
} // end namespace clang
#endif
