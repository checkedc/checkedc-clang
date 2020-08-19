//=--GatherTool.h-------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class is used to gather arguments of functions which are WILD so that
// explicit cast could be inserted when checked pointers are used as parameters
// for corresponding calls
//===----------------------------------------------------------------------===//

#ifndef _GATHERTOOL_H_
#define _GATHERTOOL_H_

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "GatherTypes.h"
#include "ProgramInfo.h"

using namespace clang;


class ArgGatherer : public ASTConsumer {
public:
  explicit ArgGatherer(ProgramInfo &I, std::string &OPostfix)
      : Info(I), OutputPostfix(OPostfix) {}
  virtual void HandleTranslationUnit(ASTContext &Context);
  ParameterMap getMF();

private:
  ProgramInfo &Info;
  std::string &OutputPostfix;
  ParameterMap MF;
};

#endif // _GATHERTOOL_H_
