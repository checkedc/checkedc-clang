//=--CastPlacement.cpp--------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains implementation of the functions and
// classes of CastPlacement.h
//===----------------------------------------------------------------------===//

#include "clang/3C/CastPlacement.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ConstraintResolver.h"
#include "clang/3C/Utils.h"
#include <clang/Tooling/Refactoring/SourceCode.h>

using namespace clang;

bool CastPlacementVisitor::VisitCallExpr(CallExpr *CE) {
  auto *FD = dyn_cast_or_null<FunctionDecl>(CE->getCalleeDecl());
  if (FD && Rewriter::isRewritable(CE->getExprLoc())) {
    // Get the constraint variable for the function.
    FVConstraint *FV = Info.getFuncConstraint(FD, Context);
    // Function has no definition i.e., external function.
    assert("Function has no definition" && FV != nullptr);

    // Did we see this function in another file?
    auto Fname = FD->getNameAsString();
    if (!ConstraintResolver::canFunctionBeSkipped(Fname)) {
      // Now we need to check the type of the arguments and corresponding
      // parameters to see if any explicit casting is needed.
      ProgramInfo::CallTypeParamBindingsT TypeVars;
      if (Info.hasTypeParamBindings(CE, Context))
        TypeVars = Info.getTypeParamBindings(CE, Context);
      unsigned PIdx = 0;
      for (const auto &A : CE->arguments()) {
        if (PIdx < FD->getNumParams()) {
          // Avoid adding incorrect casts to generic function arguments by
          // removing implicit casts when on arguments with a consistently
          // used generic type.
          Expr *ArgExpr = A;
          const TypeVariableType *TyVar =
              getTypeVariableType(FD->getParamDecl(PIdx));
          if (TyVar && TypeVars.find(TyVar->GetIndex()) != TypeVars.end() &&
              TypeVars[TyVar->GetIndex()] != nullptr)
            ArgExpr = ArgExpr->IgnoreImpCasts();

          CVarSet ArgumentConstraints = CR.getExprConstraintVars(ArgExpr);
          ConstraintVariable *ParameterC = FV->getParamVar(PIdx);
          for (auto *ArgumentC : ArgumentConstraints) {
            if (needCasting(ArgumentC, ParameterC)) {
              // We expect the cast string to end with "(".
              std::string CastString = getCastString(ArgumentC, ParameterC);
              surroundByCast(CastString, A);
              break;
            }
          }
        }
        PIdx++;
      }
    }
  }
  return true;
}

// Check whether an explicit casting is needed when the pointer represented
// by src variable is assigned to dst.
bool CastPlacementVisitor::needCasting(ConstraintVariable *Src,
                                       ConstraintVariable *Dst) {
  const auto &E = Info.getConstraints().getVariables();
  // Check if the src is a checked type.
  if (Src->isChecked(E)) {
    // If Dst has an itype, Src must have exactly the same checked type. If this
    // is not the case, we must insert a case.
    if (Dst->hasItype())
      return !Dst->solutionEqualTo(Info.getConstraints(), Src);

    // Is Dst Wild?
    // TODO: The Dinfo == WILD comparison seems to be the cause of a cast
    //       insertion bug. Can it be removed?
    if (!Dst->isChecked(E))
      return true;
  }
  return false;
}

// Get the type name to insert for casting.
std::string CastPlacementVisitor::getCastString(ConstraintVariable *Src,
                                                ConstraintVariable *Dst) {
  assert(needCasting(Src, Dst) && "No casting needed.");
  return "(" + Dst->getRewritableOriginalTy() + ")";
}

void CastPlacementVisitor::surroundByCast(const std::string &CastPrefix,
                                          Expr *E) {
  // If E is already a cast expression, we will try to rewrite the cast instead
  // of adding a new expression.
  if (auto *CE = dyn_cast<CStyleCastExpr>(E->IgnoreParens())) {
    SourceRange CastTypeRange(CE->getLParenLoc(), CE->getRParenLoc());
    Writer.ReplaceText(CastTypeRange, CastPrefix);
  } else {
    bool FrontRewritable = Writer.isRewritable(E->getBeginLoc());
    bool EndRewritable = Writer.isRewritable(E->getEndLoc());
    if (FrontRewritable && EndRewritable) {
      bool FFail = Writer.InsertTextAfterToken(E->getEndLoc(), ")");
      bool EFail = Writer.InsertTextBefore(E->getBeginLoc(), "(" + CastPrefix);
      assert("Locations were rewritable, fail should not be possible." &&
             !FFail && !EFail);
    } else {
      // This means we failed to insert the text at the end of the RHS.
      // This can happen because of Macro expansion.
      // We will see if this is a single expression statement?
      // If yes, then we will use parent statement to add ")"
      auto CRA = CharSourceRange::getTokenRange(E->getSourceRange());
      auto NewCRA = clang::Lexer::makeFileCharRange(
          CRA, Context->getSourceManager(), Context->getLangOpts());
      std::string SrcText = clang::tooling::getText(CRA, *Context);
      // Only insert if there is anything to write.
      if (!SrcText.empty())
        Writer.ReplaceText(NewCRA, "(" + CastPrefix + SrcText + ")");
    }
  }
}