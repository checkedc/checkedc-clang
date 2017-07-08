//=------ VarEquiv.cpp - Analysis of equality of variables -----*- C++ --**-==/
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  Conservatively determine for each program point in a function which 
//  variables must be equal to each other, constants, or address-expressions 
//  whose values do not vary during during the lifetime of the function.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_VAREQUIV_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_VAREQUIV_H

#include "clang/AST/Decl.h"
#include "clang/AST/CanonBounds.h"

namespace clang {

class CFG;
class CFGBlock;
class SourceManager;
  
class VarEquiv : EqualityRelation {
public:
  VarEquiv(CFG *cfg);

  void analyze();
  void setCurrentBlock(CFGBlock block);
  void moveAfterNextStmt();
  const VarDecl *getRepresentative(const VarDecl *V);

  /// Print to stderr the equivalence information associated with
  /// each basic block.
  void dumpAll(const SourceManager& M);

  /// Print to stderr the equivalence information associated with
  /// the current statement.
  void dumpCurrentStmt(const SourceManager& M);
private:
  CFG *cfg;
};
}
#endif
