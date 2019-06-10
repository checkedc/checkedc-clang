//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This is an ASTConsumer that tries to infer the CheckedC style bounds
// for identified array variables.
//===----------------------------------------------------------------------===//

#ifndef _ARRAYBOUNDSINFERENCECONSUMER_H
#define _ARRAYBOUNDSINFERENCECONSUMER_H

#include "clang/AST/ASTConsumer.h"

#include "ProgramInfo.h"

class ArrayBoundsInferenceConsumer: public clang::ASTConsumer {
public:
  explicit ArrayBoundsInferenceConsumer(ProgramInfo &I, clang::ASTContext *C) :
          Info(I) { }

  virtual void HandleTranslationUnit(clang::ASTContext &);

private:
  ProgramInfo &Info;
};

#endif //_ARRAYBOUNDSINFERENCECONSUMER_H
