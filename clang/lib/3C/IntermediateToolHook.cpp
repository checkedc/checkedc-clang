//=--IntermediateToolHook.cpp-------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of methods in IntermediateToolHook.h
//===----------------------------------------------------------------------===//

#include "clang/3C/IntermediateToolHook.h"
#include "clang/3C/ArrayBoundsInferenceConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"

using namespace llvm;
using namespace clang;

extern cl::OptionCategory ArrBoundsInferCat;
static cl::opt<bool>
    DisableArrH("disable-arr-hu",
                cl::desc("Disable Array Bounds Inference Heuristics."),
                cl::init(false), cl::cat(ArrBoundsInferCat));

void IntermediateToolHook::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);
  HandleArrayVariablesBoundsDetection(&Context, Info, !DisableArrH);
  Info.exitCompilationUnit();
}
