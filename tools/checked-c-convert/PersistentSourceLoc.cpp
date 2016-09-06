//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#include "PersistentSourceLoc.h"

using namespace clang;
using namespace llvm;

PersistentSourceLoc
PersistentSourceLoc::mkPSL(Decl *D, ASTContext &C) {
  SourceLocation SL = D->getLocation();

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) 
    SL = C.getSourceManager().getSpellingLoc(FD->getLocation());
  else if (ParmVarDecl *PV = dyn_cast<ParmVarDecl>(D)) 
    SL = C.getSourceManager().getSpellingLoc(PV->getLocation());
  else if(VarDecl *V = dyn_cast<ParmVarDecl>(D))
    SL = C.getSourceManager().getExpansionLoc(V->getLocation());
  
  return mkPSL(SL, C);
}


PersistentSourceLoc
PersistentSourceLoc::mkPSL(Stmt *S, ASTContext &Context) {
  return mkPSL(S->getLocStart(), Context);
}

PersistentSourceLoc 
PersistentSourceLoc::mkPSL(SourceLocation SL, ASTContext &Context) {
  PresumedLoc PL = Context.getSourceManager().getPresumedLoc(SL);
  
  if (!PL.isValid())
    return PersistentSourceLoc();
  SourceLocation ESL = Context.getSourceManager().getExpansionLoc(SL);
  FullSourceLoc FESL = Context.getFullLoc(ESL);
  assert(FESL.isValid());
  
  std::string fn = sys::path::filename(PL.getFilename()).str();

  PersistentSourceLoc PSL(fn, 
    FESL.getExpansionLineNumber(), FESL.getExpansionColumnNumber());

  return PSL;
}
