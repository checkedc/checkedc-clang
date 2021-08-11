//===== AvailableWhereFactsAnalysis.h - Dataflow analysis for available facts ====//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// This file implements a dataflow analysis for available facts analysis.
//===---------------------------------------------------------------------===//

#include "clang/AST/ExprUtils.h"
#include "clang/Sema/BoundsUtils.h"
#include "clang/Sema/AvailableWhereFactsAnalysis.h"

namespace clang {

//===---------------------------------------------------------------------===//
// Implementation of the methods in the AvailableWhereFactsAnalysis class. This is
// the main class that implements the dataflow analysis for available facts.
// This class uses helper methods from the AvailableFactsUtil class that are 
// defined later in this file.
//===---------------------------------------------------------------------===//

void AvailableWhereFactsAnalysis::Analyze(FunctionDecl *FD,
                                          StmtSetTy NestedStmts) {
}

void AvailableWhereFactsAnalysis::DumpAvailableFacts(FunctionDecl *FD) {
  OS << "\n--------------------------------------\n";
  // Print the function name.
  OS << "Function: " << FD->getName() << "\n";

  for (const CFGBlock *CurrBlock : GetOrderedBlocks()) {
    // Print the current block number.
    OS << "Block: B" << CurrBlock->getBlockID();

    // Print the predecessor blocks of the current block.
    OS << "; Pred: ";
    for (const CFGBlock *PredBlock : CurrBlock->preds()) {
      if (PredBlock) {
        OS << "B" << PredBlock->getBlockID();

        if (PredBlock != *(CurrBlock->preds().end() - 1))
          OS << ", ";
      }
    }

    // Print the successor blocks of the current block.
    OS << "; Succ: ";
    for (const CFGBlock *SuccBlock : CurrBlock->succs()) {
      if (SuccBlock) {
        OS << "B" << SuccBlock->getBlockID();

        if (SuccBlock != *(CurrBlock->succs().end() - 1))
          OS << ", ";
        }
    }
    OS << "\n";

    ElevatedCFGBlock *EB = BlockMap[CurrBlock];

    // Print each statement.
    for (CFGElement Elem : *CurrBlock) {
      if (Elem.getKind() == CFGElement::Statement) {
        const Stmt *CurrStmt = Elem.castAs<CFGStmt>().getStmt();
        if (!CurrStmt)
          continue;

        AFUtil.Print(CurrStmt);

        OS << "    Facts:";

        AbstractFactListTy Facts = GetStmtIn(EB, CurrStmt);

        AFUtil.DumpAbstractFacts(Facts);

        OS << "    KillVars: ";
        AFUtil.PrintVarSet(EB->StmtKill[CurrStmt]);

        OS << "\n";
      }
    }

    OS << "  Gen [B" << CurrBlock->getBlockID() << ", AllSucc]: ";
    AFUtil.DumpAbstractFacts(EB->GenAllSucc);

    OS << "\n  Kill [B" << CurrBlock->getBlockID() << ", AllSucc]: ";
    AFUtil.PrintVarSet(EB->Kill);

    OS << "\n  In [B" << CurrBlock->getBlockID() << "]: ";
    AFUtil.DumpAbstractFacts(EB->In);

    OS << "\n  Out [B" << CurrBlock->getBlockID() << ", AllSucc]: ";
    AFUtil.DumpAbstractFacts(EB->OutAllSucc);

    for (const CFGBlock *SuccBlock : CurrBlock->succs()) {
      if (AFUtil.SkipBlock(SuccBlock))
        continue;

      ElevatedCFGBlock *SuccEB = BlockMap[SuccBlock];
      
      OS << "\n  Gen ["
         << "B" << CurrBlock->getBlockID()
         << " -> "
         << "B" << SuccBlock->getBlockID()
         << "]: ";

      AFUtil.DumpAbstractFacts(EB->Gen[SuccEB]);

      OS << "\n  Out ["
         << "B" << CurrBlock->getBlockID()
         << " -> "
         << "B" << SuccBlock->getBlockID()
         << "]: ";

      AFUtil.DumpAbstractFacts(EB->Out[SuccEB]);
    }
    OS << "\n";
  }

  OS << "==-----------------------------------==\n";
}
//===---------------------------------------------------------------------===//
// Implementation of the methods in the AvailableFactsUtil class. This class
// contains helper methods that are used by the AvailableFactsUtil class to
// perform the dataflow analysis.
//===---------------------------------------------------------------------===//

void AvailableFactsUtil::Print(const Expr * E) const {
  if (!E) {
    OS << "  Expr: null\n";
    return;
  }

  std::string Str;
  llvm::raw_string_ostream SS(Str);
  static PrintingPolicy print_policy(Ctx.getPrintingPolicy());
  print_policy.FullyQualifiedName = 1;
  print_policy.SuppressScope = 0;
  print_policy.PrintCanonicalTypes = 1;
  E->printPretty(SS, nullptr, print_policy);

  OS << "  Expr: " << SS.str();
  if (SS.str().back() != '\n')
    OS << "\n";
}

void AvailableFactsUtil::Print(const Stmt *Stmt) const {
  if (!Stmt) {
    OS << "  Stmt: null\n";
    return;
  }

  std::string Str;
  llvm::raw_string_ostream SS(Str);
  Stmt->printPretty(SS, nullptr, Ctx.getPrintingPolicy());

  OS << "  Stmt: " << SS.str();
  if (SS.str().back() != '\n')
    OS << "\n";
}

void AvailableFactsUtil::DumpAbstractFact(const AbstractFact *AFact) const {
  if (auto *Fact = dyn_cast<WhereClauseFact>(AFact)) {
    if (auto *BF = dyn_cast<BoundsDeclFact>(Fact)) {
      if (const BoundsExpr *Bounds = BF->getBoundsExpr()) {
        BoundsExpr *NormalizedBounds = SemaRef.NormalizeBounds(BF);
        RangeBoundsExpr *RBE = dyn_cast_or_null<RangeBoundsExpr>(NormalizedBounds);

        if (Bounds == AvailableWhereFactsAnalysis::Top) {
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
  } 
  else if (auto *IF = dyn_cast<InferredFact>(AFact)) {
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

void AvailableFactsUtil::DumpAbstractFacts(const AbstractFactListTy &Facts) const {
  size_t s = Facts.size();
  if (s == 0) {
    OS << "{}\n";
  } else {
    OS << "\n";
  }

  for (const AbstractFact *AFact : Facts) {
    DumpAbstractFact(AFact);
  }
}

void AvailableFactsUtil::PrintVarSet(VarSetTy VarSet) const {
  if (VarSet.size() == 0) {
    OS << "{}\n";
    return;
  }

  // A VarSetTy has const iterator. So we cannot simply sort a VarSetTy and
  // need to copy the elements to a vector to sort.
  std::vector<const VarDecl *> Vars(VarSet.begin(), VarSet.end());

  llvm::sort(Vars.begin(), Vars.end(),
    [](const VarDecl *A, const VarDecl *B) {
       return A->getQualifiedNameAsString().compare(
              B->getQualifiedNameAsString()) < 0;
    });

  for (const VarDecl *V : Vars)
    OS << V->getQualifiedNameAsString() << ", ";
  OS << "\n";
}

// end of methods for the AvailableWhereFactsAnalysis class.

} // end namespace clang
