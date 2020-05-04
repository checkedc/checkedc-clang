//=--GatherTool.cpp-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of methods in GatherTool.h
//===----------------------------------------------------------------------===//

#include "clang/AST/RecursiveASTVisitor.h"

#include "clang/AST/Type.h"
#include "clang/CConv/GatherTool.h"

using namespace llvm;
using namespace clang;


class ParameterGatherer : public clang::RecursiveASTVisitor<ParameterGatherer> {
public:
  explicit ParameterGatherer(ASTContext *_C, ProgramInfo &_I, ParameterMap &_MF)
      : Context(_C), Info(_I), MF(_MF) {}

  bool VisitFunctionDecl(FunctionDecl *FD) {
    auto Fn = FD->getNameAsString();
    bool AnExternFunction = FD->isGlobal() && Info.isAnExternFunction(Fn);
    if (FD->doesThisDeclarationHaveABody() || AnExternFunction) {
      std::vector<IsChecked> ParmTypes;
      int Pi = 0;
      auto &CS = Info.getConstraints();
      for (auto &Param : FD->parameters()) {
        bool IsWild = false;
        std::set<ConstraintVariable *> Cvs = Info.getVariable(Param,
                                                              Context, FD, Pi);
        for (auto Cv : Cvs) {
          IsWild |= Cv->hasWild(CS.getVariables());
          // If this an extern function, then check if there is
          // any explicit annotation to. If not? then add a cast.
          if (AnExternFunction && !IsWild) {
            if (PVConstraint *PV = dyn_cast<PVConstraint>(Cv)) {
              for (auto cKey : PV->getCvars()) {
                if (VarAtom *VA = dyn_cast<VarAtom>(cKey)) {
                  IsWild = true;
                  break;
                }
              }
            }
          }
        }
        ParmTypes.push_back(IsWild ? WILD : CHECKED);
        Pi++;
      }
      MF[Fn] = ParmTypes;
    }

    return false;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  ParameterMap &MF;
};

void ArgGatherer::HandleTranslationUnit(ASTContext &Context) {
  Info.enterCompilationUnit(Context);
  ParameterGatherer PG(&Context, Info, MF);
  for (auto &D : Context.getTranslationUnitDecl()->decls()) {
    PG.TraverseDecl(D);
  }

  Info.merge_MF(MF);

  Info.exitCompilationUnit();
}

ParameterMap ArgGatherer::getMF() {
  return MF;
}
