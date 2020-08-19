//=--CastPlacement.cpp---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains implementation of the functions and
// classes of CastPlacement.h
//===----------------------------------------------------------------------===//

#include "clang/CConv/ConstraintResolver.h"
#include "clang/CConv/RewriteUtils.h"
#include "clang/CConv/Utils.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CConv/ArrayBoundsInferenceConsumer.h"
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/MappingVisitor.h"
#include "clang/CConv/CastPlacement.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Tooling/Refactoring/SourceCode.h"
#include <sstream>

using namespace clang;


bool CastPlacementVisitor::VisitCallExpr(CallExpr *CE) {
  Decl *D = CE->getCalleeDecl();
  if (Rewriter::isRewritable(CE->getExprLoc()) && D) {
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(CE, *Context);
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      // Get the constraint variable for the function.
      std::set<FVConstraint *> *V = Info.getFuncConstraints(FD, Context);
      // Function has no definition i.e., external function.
      assert(V);
//        if (V == nullptr) {
//          V = Info.getFuncDeclConstraints(FD, Context);
//        }
      // TODO Deubgging lines
      // llvm::errs() << "Decl for: " << FD->getNameAsString() << "\nVars:";
      // for (auto &CV : V) {
      //   CV->dump();
      //   llvm::errs() << "\n";
      // }

      // Did we see this function in another file?
      auto Fname = FD->getNameAsString();
      auto PInfo = Info.get_MF()[Fname];

      if (V != nullptr && V->size() > 0 &&
          !ConstraintResolver::canFunctionBeSkipped(Fname)) {
        // Get the FV constraint for the Callee.
        FVConstraint *FV = *(V->begin());
        // Now we need to check the type of the arguments and corresponding
        // parameters to see, if any explicit casting is needed.
        if (FV) {
          ProgramInfo::CallTypeParamBindingsT TypeVars;
          if (Info.hasTypeParamBindings(CE, Context))
            TypeVars = Info.getTypeParamBindings(CE, Context);
          unsigned i = 0;
          for (const auto &A : CE->arguments()) {
            if (i < FD->getNumParams()) {

              // Avoid adding incorrect casts to generic function arguments by
              // removing implicit casts when on arguments with a consistently
              // used generic type.
              CVarSet ArgumentConstraints;
              const TypeVariableType
                  *TyVar = getTypeVariableType(FD->getParamDecl(i));
              if (TyVar && TypeVars.find(TyVar->GetIndex()) != TypeVars.end()
                  && TypeVars[TyVar->GetIndex()] != nullptr)
                ArgumentConstraints =
                    CR.getExprConstraintVars(A->IgnoreImpCasts());
              else
                ArgumentConstraints = CR.getExprConstraintVars(A);
              CVarSet &ParameterConstraints =
                  FV->getParamVar(i);
              bool CastInserted = false;
              for (auto *ArgumentC : ArgumentConstraints) {
                CastInserted = false;
                for (auto *ParameterC : ParameterConstraints) {
                  auto Dinfo = i < PInfo.size() ? PInfo[i] : CHECKED;
                  if (needCasting(ArgumentC, ParameterC, Dinfo)) {
                    // We expect the cast string to end with "(".
                    std::string CastString =
                        getCastString(ArgumentC, ParameterC, Dinfo);
                    surroundByCast(CastString, A);
                    CastInserted = true;
                    break;
                  }
                }
                // If we have already inserted a cast, then break.
                if (CastInserted) break;
              }

            }
            i++;
          }
        }
      }
    }
  }
  return true;
}

  
// Check whether an explicit casting is needed when the pointer represented
// by src variable is assigned to dst.
bool CastPlacementVisitor::needCasting(ConstraintVariable *Src,
                                       ConstraintVariable *Dst,
                                       IsChecked Dinfo) {
  auto &E = Info.getConstraints().getVariables();
  auto SrcChecked = Src->isChecked(E);
  // Check if the src is a checked type.
  if (SrcChecked) {
    // Check if Dst is an itype, if yes then
    // Src should have exactly same checked type else we need to insert cast.
    if (Dst->hasItype()) {
    return !Dst->solutionEqualTo(Info.getConstraints(), Src);
    }

    // Is Dst Wild?
    if (!Dst->isChecked(E) || Dinfo == WILD) {
      return true;
    }

  } return false; }

// Get the type name to insert for casting.
std::string CastPlacementVisitor::getCastString(ConstraintVariable *Src,
                                                ConstraintVariable *Dst,
                                                IsChecked Dinfo) {
  assert(needCasting(Src, Dst, Dinfo) && "No casting needed.");
  // The destination type should be a non-checked type.
  // This is not necessary because of itypes
  //auto &E = Info.getConstraints().getVariables();
  //assert(!Dst->anyChanges(E) || Dinfo == WILD);
  return "((" + Dst->getRewritableOriginalTy() + ")";
}

void CastPlacementVisitor::surroundByCast(std::string CastPrefix, Expr *E) {
  if (Writer.InsertTextAfterToken(E->getEndLoc(), ")")) {
    // This means we failed to insert the text at the end of the RHS.
    // This can happen because of Macro expansion.
    // We will see if this is a single expression statement?
    // If yes, then we will use parent statement to add ")"
    auto CRA = CharSourceRange::getTokenRange(E->getSourceRange());
    auto NewCRA = clang::Lexer::makeFileCharRange(CRA,
                                                Context->getSourceManager(),
                                                  Context->getLangOpts());
    std::string SrcText = clang::tooling::getText(CRA, *Context);
    // Only insert if there is anything to write.
    if (!SrcText.empty())
      Writer.ReplaceText(NewCRA, CastPrefix + SrcText + ")");
  } else {
    Writer.InsertTextBefore(E->getBeginLoc(), CastPrefix);
  }
}
