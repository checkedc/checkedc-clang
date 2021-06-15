//=--PersistentSourceLoc.h----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class specifies a location in a source file that persists across
// invocations of the frontend. Given a Decl/Stmt/Expr, the FullSourceLoc
// of that value can be compared with an instance of this class for
// equality. If they are equal, then you can substitute the Decl/Stmt/Expr
// for the instance of this class.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_PERSISTENTSOURCELOC_H
#define LLVM_CLANG_3C_PERSISTENTSOURCELOC_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

class PersistentSourceLoc {
protected:
  PersistentSourceLoc(std::string F, uint32_t L, uint32_t C, uint32_t E)
      : FileName(F), LineNo(L), ColNoS(C), ColNoE(E), IsValid(true) {}

public:
  PersistentSourceLoc()
      : FileName(""), LineNo(0), ColNoS(0), ColNoE(0), IsValid(false) {}
  std::string getFileName() const { return FileName; }
  uint32_t getLineNo() const { return LineNo; }
  uint32_t getColSNo() const { return ColNoS; }
  uint32_t getColENo() const { return ColNoE; }
  bool valid() const { return IsValid; }

  bool operator<(const PersistentSourceLoc &O) const {
    if (FileName == O.FileName)
      if (LineNo == O.LineNo)
        if (ColNoS == O.ColNoS)
          if (ColNoE == O.ColNoE)
            return false;
          else
            return ColNoE < O.ColNoE;
        else
          return ColNoS < O.ColNoS;
      else
        return LineNo < O.LineNo;
    else
      return FileName < O.FileName;
  }

  void print(llvm::raw_ostream &O) const {
    O << FileName << ":" << LineNo << ":" << ColNoS << ":" << ColNoE;
  }

  void dump() const { print(llvm::errs()); }

  static PersistentSourceLoc mkPSL(const clang::Decl *D,
                                   clang::ASTContext &Context);

  static PersistentSourceLoc mkPSL(const clang::Stmt *S,
                                   clang::ASTContext &Context);

  static PersistentSourceLoc mkPSL(const clang::Expr *E,
                                   clang::ASTContext &Context);

private:
  // Create a PersistentSourceLoc based on absolute file path
  // from the given SourceRange and SourceLocation.
  static PersistentSourceLoc mkPSL(clang::SourceRange SR,
                                   clang::SourceLocation SL,
                                   clang::ASTContext &Context);
  // The source file name.
  std::string FileName;
  // Starting line number.
  uint32_t LineNo;
  // Column number start.
  uint32_t ColNoS;
  // Column number end.
  uint32_t ColNoE;
  bool IsValid;
};

typedef std::pair<PersistentSourceLoc, PersistentSourceLoc>
    PersistentSourceRange;

#endif
