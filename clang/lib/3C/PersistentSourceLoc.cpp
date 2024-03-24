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
PersistentSourceLoc PersistentSourceLoc::mkPSL(const Decl *D,
                                               const ASTContext &C) {
  if (D == nullptr) return PersistentSourceLoc();
  SourceLocation SL = C.getSourceManager().getExpansionLoc(D->getLocation());
  return mkPSL(D->getSourceRange(), SL, C);
}

// Create a PersistentSourceLoc for a Stmt.
PersistentSourceLoc PersistentSourceLoc::mkPSL(const Stmt *S,
                                               const ASTContext &Context) {
  if (S == nullptr) return PersistentSourceLoc();
  return mkPSL(S->getSourceRange(), S->getBeginLoc(), Context);
}

// Create a PersistentSourceLoc for an Expression.
PersistentSourceLoc PersistentSourceLoc::mkPSL(const clang::Expr *E,
                                               const clang::ASTContext &Context) {
  if (E == nullptr) return PersistentSourceLoc();
  return mkPSL(E->getSourceRange(), E->getBeginLoc(), Context);
}

// Use the PresumedLoc infrastructure to get a file name and expansion
// line and column numbers for a SourceLocation.
PersistentSourceLoc PersistentSourceLoc::mkPSL(clang::SourceRange SR,
                                               SourceLocation SL,
                                               const ASTContext &Context) {
  const SourceManager &SM = Context.getSourceManager();
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
  std::string Fn = PL.getFilename();

  // Get the absolute filename of the file.
  FullSourceLoc TFSL(SR.getBegin(), SM);
  if (TFSL.isValid()) {
    const FileEntry *Fe = SM.getFileEntryForID(TFSL.getFileID());
    std::string FeAbsS = Fn;
    if (Fe != nullptr) {
      // Unlike in `emit` in RewriteUtils.cpp, we don't re-canonicalize the file
      // path because of the potential performance cost (mkPSL is called on many
      // AST nodes in each translation unit) and because we don't have a good
      // way to handle errors. If there is a problem, `emit` will detect it
      // before we actually write a file.
      FeAbsS = Fe->tryGetRealPathName().str();
    }
    Fn = std::string(sys::path::remove_leading_dotslash(FeAbsS));
  }
  PersistentSourceLoc PSL(Fn, FESL.getExpansionLineNumber(),
                          FESL.getExpansionColumnNumber(), EndCol);

  return PSL;
}
