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

  struct PrepassInfo {
    // VarUses maps each VarDecl V in a function to the DeclRefExpr (if any)
    // that is the first use of V, if V fulfills the following conditions:
    // 1. V is used in a declared bounds expression, or:
    // 2. V has a declared bounds expression.
    VarUsageTy VarUses;
  };
} // end namespace clang
#endif
