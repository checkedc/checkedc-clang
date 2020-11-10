//=--PersistentSourceLoc.cpp--------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of the PersistentSourceLoc infrastructure.
//===----------------------------------------------------------------------===//

#include "clang/3C/PersistentSourceLoc.h"
#include "clang/3C/Utils.h"

using namespace clang;
using namespace llvm;

// Given a Decl, look up the source location for that Decl and create a
// PersistentSourceLoc that represents the location of the Decl.
// This currently the expansion location for the declarations source location.
// If we want to add more complete source for macros in the future, I expect we
// will need to the spelling location instead.
PersistentSourceLoc PersistentSourceLoc::mkPSL(const Decl *D, ASTContext &C) {
  SourceLocation SL = C.getSourceManager().getExpansionLoc(D->getLocation());
  return mkPSL(D->getSourceRange(), SL, C);
}

// Create a PersistentSourceLoc for a Stmt.
PersistentSourceLoc PersistentSourceLoc::mkPSL(const Stmt *S,
                                               ASTContext &Context) {
  return mkPSL(S->getSourceRange(), S->getBeginLoc(), Context);
}

// Create a PersistentSourceLoc for an Expression.
PersistentSourceLoc PersistentSourceLoc::mkPSL(const clang::Expr *E,
                                               clang::ASTContext &Context) {
  return mkPSL(E->getSourceRange(), E->getBeginLoc(), Context);
}

// Use the PresumedLoc infrastructure to get a file name and expansion
// line and column numbers for a SourceLocation.
PersistentSourceLoc PersistentSourceLoc::mkPSL(clang::SourceRange SR,
                                               SourceLocation SL,
                                               ASTContext &Context) {
  SourceManager &SM = Context.getSourceManager();
  PresumedLoc PL = SM.getPresumedLoc(SL);

  // If there is no PresumedLoc, create a nullary PersistentSourceLoc.
  if (!PL.isValid())
    return PersistentSourceLoc();

  SourceLocation ESL = SM.getExpansionLoc(SL);
  FullSourceLoc FESL = Context.getFullLoc(ESL);

  assert(FESL.isValid());
  // Get End location, if exists.
  uint32_t EndCol = 0;
  if (SR.getEnd().isValid() && SM.getExpansionLoc(SR.getEnd()).isValid()) {
    FullSourceLoc EFESL = Context.getFullLoc(SM.getExpansionLoc(SR.getEnd()));
    if (EFESL.isValid()) {
      EndCol = EFESL.getExpansionColumnNumber();
    }
  }
  std::string fn = PL.getFilename();

  // Get the absolute filename of the file.
  FullSourceLoc tFSL(SR.getBegin(), SM);
  if (tFSL.isValid()) {
    const FileEntry *fe = SM.getFileEntryForID(tFSL.getFileID());
    std::string toConv = fn;
    std::string feAbsS = "";
    if (fe != nullptr)
      toConv = fe->getName();
    if (getAbsoluteFilePath(toConv, feAbsS))
      fn = sys::path::remove_leading_dotslash(feAbsS);
  }
  PersistentSourceLoc PSL(fn, FESL.getExpansionLineNumber(),
                          FESL.getExpansionColumnNumber(), EndCol);

  return PSL;
}
