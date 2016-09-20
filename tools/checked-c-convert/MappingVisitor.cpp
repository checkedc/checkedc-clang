//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementations of the MappingVisitor functions for VisitStmt and VisitDecl.
//===----------------------------------------------------------------------===//
#include "MappingVisitor.h"
#include "llvm/Support/Path.h"

using namespace clang;
using namespace llvm;

bool MappingVisitor::VisitDeclStmt(DeclStmt *S) {
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(S, Context);

  if (PSL.valid()) {

    // Check to see if the source location as described by the current location
    // of S appears in the set of PersistentSourceLocs we are tasked to 
    // resolve. If it is, then create a mapping mapping the current 
    // PersistentSourceLocation to the Stmt object S.
    std::set<PersistentSourceLoc>::iterator I = SourceLocs.find(PSL);
    if (I != SourceLocs.end()) {
      Decl *D = NULL;
      Stmt *So = NULL;
      Type *T = NULL;
      std::tie<Stmt *, Decl *, Type *>(So, D, T) = PSLtoSDT[PSL];
      if (So != NULL && Verbose) {
        errs() << "\nOverriding ";
        S->dump();
        errs() << "\n";
        errs() << "With ";
        So->dump();
        errs() << "\n";
        errs() << " at ";
        PSL.dump();
        errs() << "\n";
      }

      if(So == NULL)
        PSLtoSDT[PSL] = StmtDeclOrType(S, D, T);
    }

    if (DeclStmt *DL = dyn_cast<DeclStmt>(S)) {
      if (DL->isSingleDecl()) {
        if (VarDecl *VD = dyn_cast<VarDecl>(DL->getSingleDecl()))
          DeclToDeclStmt[VD] = DL;
      }
      else
        for (auto I : DL->decls())
          DeclToDeclStmt[I] = DL;
    }
  }
  
  return true;
}

bool MappingVisitor::VisitDecl(Decl *D) {
  PersistentSourceLoc PSL = 
    PersistentSourceLoc::mkPSL(D, Context);
  if (PSL.valid()) {
    std::set<PersistentSourceLoc>::iterator I = SourceLocs.find(PSL);
    if (I != SourceLocs.end()) {
      Decl *Do = NULL;
      Stmt *S = NULL;
      Type *T = NULL;
      std::tie<Stmt *, Decl *, Type *>(S, Do, T) = PSLtoSDT[PSL];
      if (Do != NULL && Verbose) {
        errs() << "Overriding ";
        Do->dump();
        errs() << " with ";
        D->dump();
        errs() << " from source location data (they are defined in";
        errs() << " the same location";
      }
      
      if(Do == NULL)
        PSLtoSDT[PSL] = StmtDeclOrType(S, D, T);
    }
  }

  return true;
}
