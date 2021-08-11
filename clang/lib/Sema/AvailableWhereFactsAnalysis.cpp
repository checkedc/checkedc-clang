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
}
// end of methods for the AvailableWhereFactsAnalysis class.

} // end namespace clang
