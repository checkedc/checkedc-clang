//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of the PersistentSourceLoc infrastructure.
//===----------------------------------------------------------------------===//
#include "PersistentSourceLoc.h"
#include "Utils.h"

using namespace clang;
using namespace llvm;

// Given a Decl, look up the source location for that Decl and create a 
// PersistentSourceLoc that represents the location of the Decl. 
// For Function and Parameter Decls, use the Spelling location, while for
// variables, use the expansion location. 
PersistentSourceLoc
PersistentSourceLoc::mkPSL(const Decl *D, ASTContext &C) {
  SourceLocation SL = D->getLocation();

  if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) 
    SL = C.getSourceManager().getSpellingLoc(FD->getLocation());
  else if (const ParmVarDecl *PV = dyn_cast<ParmVarDecl>(D)) 
    SL = C.getSourceManager().getSpellingLoc(PV->getLocation());
  else if(const VarDecl *V = dyn_cast<VarDecl>(D))
    SL = C.getSourceManager().getExpansionLoc(V->getLocation());
  
  return mkPSL(D->getSourceRange(), SL, C);
}


// Create a PersistentSourceLoc for a Stmt.
PersistentSourceLoc
PersistentSourceLoc::mkPSL(const Stmt *S, ASTContext &Context) {
  return mkPSL(S->getSourceRange(), S->getBeginLoc(), Context);
}

// Use the PresumedLoc infrastructure to get a file name and expansion
// line and column numbers for a SourceLocation.
PersistentSourceLoc 
PersistentSourceLoc::mkPSL(clang::SourceRange SR, SourceLocation SL, ASTContext &Context) {
  SourceManager &SM = Context.getSourceManager();
  PresumedLoc PL = SM.getPresumedLoc(SL);

  // If there is no PresumedLoc, create a nullary PersistentSourceLoc.  
  if (!PL.isValid())
    return PersistentSourceLoc();

  SourceLocation ESL = SM.getExpansionLoc(SL);
  FullSourceLoc FESL = Context.getFullLoc(ESL);
  assert(FESL.isValid());
  std::string fn = PL.getFilename();

  // get the absolute filename of the file
  FullSourceLoc tFSL(SR.getBegin(), SM);
  const FileEntry *fe = SM.getFileEntryForID(tFSL.getFileID());
  std::string feAbsS = "";
  if(fe != nullptr && getAbsoluteFilePath(fe->getName(), feAbsS)) {
    fn = sys::path::remove_leading_dotslash(feAbsS);
  }

  PersistentSourceLoc PSL(fn, 
    FESL.getExpansionLineNumber(), FESL.getExpansionColumnNumber());

  return PSL;
}
