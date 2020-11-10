//=--MappingVisitor.cpp-------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementations of the MappingVisitor functions for VisitStmt and VisitDecl.
//===----------------------------------------------------------------------===//

#include "llvm/Support/Path.h"

#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/MappingVisitor.h"

using namespace clang;

bool MappingVisitor::VisitDeclStmt(DeclStmt *S) {
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(S, Context);

  if (PSL.valid()) {

    // Check to see if the source location as described by the current location
    // of S appears in the set of PersistentSourceLocs we are tasked to
    // resolve. If it is, then create a mapping mapping the current
    // PersistentSourceLocation to the Stmt object S.
    std::set<PersistentSourceLoc>::iterator I = SourceLocs.find(PSL);
    if (I != SourceLocs.end()) {
      Decl *D = nullptr;
      Stmt *So = nullptr;
      std::tie<Stmt *, Decl *>(So, D) = PSLtoSDT[PSL];
      if (So != nullptr && Verbose) {
        llvm::errs() << "\nOverriding ";
        S->dump();
        llvm::errs() << "\n";
        llvm::errs() << "With ";
        So->dump();
        llvm::errs() << "\n";
        llvm::errs() << " at ";
        PSL.dump();
        llvm::errs() << "\n";
      }

      if (So == nullptr)
        PSLtoSDT[PSL] = StmtDecl(S, D);
    }

    if (DeclStmt *DL = dyn_cast<DeclStmt>(S)) {
      if (DL->isSingleDecl()) {
        if (VarDecl *VD = dyn_cast<VarDecl>(DL->getSingleDecl()))
          DeclToDeclStmt[VD] = DL;
      } else
        for (auto I : DL->decls())
          DeclToDeclStmt[I] = DL;
    }
  }

  return true;
}

bool MappingVisitor::VisitDecl(Decl *D) {
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(D, Context);
  if (PSL.valid()) {
    std::set<PersistentSourceLoc>::iterator I = SourceLocs.find(PSL);
    if (I != SourceLocs.end()) {
      Decl *Do = nullptr;
      Stmt *S = nullptr;
      std::tie<Stmt *, Decl *>(S, Do) = PSLtoSDT[PSL];
      if (Do != nullptr && Verbose) {
        llvm::errs() << "Overriding ";
        Do->dump();
        llvm::errs() << " with ";
        D->dump();
        llvm::errs() << " from source location data (they are defined in";
        llvm::errs() << " the same location";
      }

      if (Do == nullptr)
        PSLtoSDT[PSL] = StmtDecl(S, D);
    }
  }

  return true;
}
