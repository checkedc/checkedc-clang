//=--IntermediateToolHook.h---------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class provides an intermediate hook for any visitors that need to be
// run after constraint solving but before rewriting, such as trying out
// heuristics in the case of array bounds inference.
//===----------------------------------------------------------------------===//

#ifndef _INTERMEDIATETOOLHOOK_H
#define _INTERMEDIATETOOLHOOK_H

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "ProgramInfo.h"

using namespace clang;

class IntermediateToolHook : public ASTConsumer {
public:
  explicit IntermediateToolHook(ProgramInfo &I, clang::ASTContext *C)
      : Info(I) {}
  virtual void HandleTranslationUnit(ASTContext &Context);

private:
  ProgramInfo &Info;
};

#endif // _INTERMEDIATETOOLHOOK_H
