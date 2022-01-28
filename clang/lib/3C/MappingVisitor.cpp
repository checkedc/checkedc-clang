//=--MappingVisitor.cpp-------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementations of the MappingVisitor functions for VisitStmt and VisitDecl.
//===----------------------------------------------------------------------===//

#include "clang/3C/MappingVisitor.h"
#include "clang/3C/3CGlobalOptions.h"
#include "llvm/Support/Path.h"

using namespace clang;

bool MappingVisitor::VisitDecl(Decl *D) {
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(D, Context);
  if (PSL.valid()) {
    std::set<PersistentSourceLoc>::iterator I = SourceLocs.find(PSL);
    if (I != SourceLocs.end()) {
      Decl *Do = PSLtoSDT[PSL];
      if (Do != nullptr && _3COpts.Verbose) {
        llvm::errs() << "Overriding ";
        Do->dump();
        llvm::errs() << " with ";
        D->dump();
        llvm::errs() << " from source location data (they are defined in";
        llvm::errs() << " the same location";
      }

      if (Do == nullptr)
        PSLtoSDT[PSL] = D;
    }
  }

  return true;
}
