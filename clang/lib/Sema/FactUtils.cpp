//===----------- FactUtils.cpp: Utility functions for facts ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===--------------------------------------------------------------------===//
//
//  This file implements utility functions for facts.
//
//===--------------------------------------------------------------------===//

#include "clang/Sema/FactUtils.h"

using namespace clang;

void FactPrinter::PrintPretty(Sema &S, const AbstractFact *AFact) {
  llvm::raw_ostream &OS = llvm::outs();
  ASTContext &Ctx = S.Context;

  if (auto *Fact = dyn_cast<WhereClauseFact>(AFact)) {
    if (auto *BF = dyn_cast<BoundsDeclFact>(Fact)) {
      if (const BoundsExpr *Bounds = BF->getBoundsExpr()) {
        BoundsExpr *NormalizedBounds = S.NormalizeBounds(BF);
        RangeBoundsExpr *RBE = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds);

        if (Bounds == BoundsTop) {
          OS << "Top\n";
          return;
        }

        Expr *Lower = RBE->getLowerExpr();
        Expr *Upper = RBE->getUpperExpr();

        OS << "     " << BF->getVarDecl()->getQualifiedNameAsString() << ": bounds(";
          Lower->printPretty(OS, nullptr, Ctx.getPrintingPolicy());
          OS << ", ";
          Upper->printPretty(OS, nullptr, Ctx.getPrintingPolicy());
        OS << ")";
      }
    } else if (auto *EF = dyn_cast<EqualityOpFact>(Fact)) {
      if (const BinaryOperator *BO = EF->EqualityOp) {
        std::string Str;
        llvm::raw_string_ostream SS(Str);

        BO->printPretty(SS, nullptr, Ctx.getPrintingPolicy());
        OS << "     " << SS.str();
      }
    }
  } else if (auto *IF = dyn_cast<InferredFact>(AFact)) {
    if (const BinaryOperator *BO = IF->EqualityOp) {
      std::string Str;
      llvm::raw_string_ostream SS(Str);
        
      BO->printPretty(SS, nullptr, Ctx.getPrintingPolicy());
      OS << "     " << SS.str();
    }
  } else {
    OS << "Unknown Fact";
  }

  OS << "\n";
}

void FactPrinter::PrintPretty(Sema &S, const AbstractFactListTy &Facts) {
  llvm::raw_ostream &OS = llvm::outs();
  
  size_t s = Facts.size();
  if (s == 0) {
    OS << "{}\n";
  } else {
    OS << "\n";
  }

  for (const AbstractFact *AFact : Facts) {
    PrintPretty(S, AFact);
  }
}
