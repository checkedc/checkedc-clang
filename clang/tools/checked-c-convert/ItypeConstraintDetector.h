//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//

#ifndef _ITYPECONSTRAINTDETECTOR_H
#define _ITYPECONSTRAINTDETECTOR_H

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "ProgramInfo.h"

using namespace clang;

class ItypeDetectorConsumer : public ASTConsumer {
public:
  explicit ItypeDetectorConsumer(ProgramInfo &I, clang::ASTContext *C) :
  Info(I) { }

  virtual void HandleTranslationUnit(clang::ASTContext &);

private:
  ProgramInfo &Info;
};

#endif //_ITYPECONSTRAINTDETECTOR_H
