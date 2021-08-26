//===----------- FactUtils.h: Utility functions for facts ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//
//
//  This file defines the utility functions for facts.
//
//===------------------------------------------------------------------===//

#ifndef LLVM_CLANG_FACTUTILS_H
#define LLVM_CLANG_FACTUTILS_H

#include "clang/AST/Expr.h"
#include "clang/Sema/Sema.h"

namespace clang {

class FactPrinter {
public:
  // TODO: should not be here
  // Top is a special bounds expression that denotes the super set of all
  // bounds expressions.
  static constexpr RangeBoundsExpr *BoundsTop = nullptr;

  // Pretty print an abstract fact.
  static void PrintPretty(Sema &S, const AbstractFact *Fact);

  // Pretty print a list of abstract fact.
  static void PrintPretty(Sema &S, const AbstractFactListTy &Facts);
};

} // end namespace clang

#endif