//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This class specifies a location in a source file that persists across
// invocations of the frontend. Given a Decl/Stmt/Expr, the FullSourceLoc
// of that value can be compared with an instance of this class for 
// equality. If they are equal, then you can substitute the Decl/Stmt/Expr
// for the instance of this class. 
//===----------------------------------------------------------------------===//
#ifndef _PERSISTENT_SOURCE_LOC_H
#define _PERSISTENT_SOURCE_LOC_H
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"

class PersistentSourceLoc {
protected:
  PersistentSourceLoc(std::string f, uint32_t l, uint32_t c) :
    fileName(f), lineNo(l), colNo(c), isValid(true) {}
  
public:
  PersistentSourceLoc() : fileName(""), lineNo(0), colNo(0), isValid(false) {}
  std::string getFileName() const { return fileName; }
  uint32_t getLineNo() const { return lineNo; }
  uint32_t getColNo() const { return colNo; }
  bool valid() { return isValid; }

  bool operator<(const PersistentSourceLoc &o) const {
    if (fileName == o.fileName)
      if (lineNo == o.lineNo)
        if (colNo == o.colNo)
          return false;
        else
          return colNo < o.colNo;
      else
        return lineNo < o.lineNo;
    else
      return fileName < o.fileName;
  }

  void print(llvm::raw_ostream &O) const {
    O << fileName << ":" << lineNo << ":" << colNo;
  }

  void dump() const { print(llvm::errs()); }

  static
    PersistentSourceLoc mkPSL(const clang::Decl *D, clang::ASTContext &Context);

  static
    PersistentSourceLoc mkPSL(const clang::Stmt *S, clang::ASTContext &Context);

private:
  static
    PersistentSourceLoc mkPSL(clang::SourceRange SR, clang::SourceLocation SL, clang::ASTContext &Context);
  std::string fileName;
  uint32_t lineNo;
  uint32_t colNo;
  bool isValid;
};

typedef std::pair<PersistentSourceLoc, PersistentSourceLoc>
PersistentSourceRange;

#endif
