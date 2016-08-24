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
PersistentSourceLoc::mkPSL(SourceLocation SL, ASTContext &Context) {
  FullSourceLoc FSL = Context.getFullLoc(SL);

  if (!FSL.isValid())
    return PersistentSourceLoc();

  assert(FSL.isValid());
  //TODO: this should go back to being the spelling loc..??
  //      why can you only look up the FileEntry from a spelling loc??
  clang::FileID fID = Context.getSourceManager().getFileID(SL);
  if (!fID.isValid()) {
    FSL.dump();
  }
  assert(fID.isValid());

  const clang::FileEntry *FE = Context.getSourceManager().getFileEntryForID(fID);

  std::string fn = "";

  if (!FE) {
    fn = "<built-in>";
  }
  else {
    assert(FE->isValid());
    fn = llvm::sys::path::filename(FE->getName()).str();
  }

  PersistentSourceLoc PSL(fn, FSL.getExpansionLineNumber(), FSL.getExpansionColumnNumber());

  return PSL;
}
