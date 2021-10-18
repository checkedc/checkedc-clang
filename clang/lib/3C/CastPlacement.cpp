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
#include "clang/Tooling/Transformer/SourceCode.h"

using namespace clang;

bool CastPlacementVisitor::VisitCallExpr(CallExpr *CE) {
  // Get the constraint variable for the function.
  Decl *CalleeDecl = CE->getCalleeDecl();
  FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(CalleeDecl);

  // Find a FVConstraint for this call. If there is more than one, then they
  // will have been unified during constraint generation, so we can use any of
  // them.
  FVConstraint *FV = nullptr;
  for (auto *CV : CR.getCalleeConstraintVars(CE)) {
    if (isa<FVConstraint>(CV))
      FV = cast<FVConstraint>(CV);
    else if (isa<PVConstraint>(CV) && cast<PVConstraint>(CV)->getFV())
      FV = cast<PVConstraint>(CV)->getFV();
    if (FV)
      break;
  }

  // Note: I'm not entirely sure that this will always hold. The previous
  // implementation just returned early if FV was null, but I don't think that
  // can ever actually happen.
  assert("Could not find function constraint variable!" && FV != nullptr);

  // Now we need to check the type of the arguments and corresponding
  // parameters to see if any explicit casting is needed.
  ProgramInfo::CallTypeParamBindingsT TypeVars;
  if (Info.hasTypeParamBindings(CE, Context))
    TypeVars = Info.getTypeParamBindings(CE, Context);

  // Cast on arguments.
  unsigned PIdx = 0;
  for (const auto &A : CE->arguments()) {
    if (PIdx < FV->numParams()) {
      // Avoid adding incorrect casts to generic function arguments by
      // removing implicit casts when on arguments with a consistently
      // used generic type.
      ConstraintVariable *TypeVar = nullptr;
      Expr *ArgExpr = A;
      if (FD && PIdx < FD->getNumParams()) {
        const int TyVarIdx = FV->getExternalParam(PIdx)->getGenericIndex();
        // Check if local type vars are available
        if (TypeVars.find(TyVarIdx) != TypeVars.end()) {
          TypeVar = TypeVars[TyVarIdx].getConstraint(
                  Info.getConstraints().getVariables());
        }
      }
      if (TypeVar != nullptr)
        ArgExpr = ArgExpr->IgnoreImpCasts();

      CVarSet ArgConstraints = CR.getExprConstraintVarsSet(ArgExpr);
      for (auto *ArgC : ArgConstraints) {
        // If the function takes a void *, we already know about the wildness,
        // so allow the implicit cast.
        if (TypeVar == nullptr && FV->getExternalParam(PIdx)->isVoidPtr())
          continue;
        CastNeeded CastKind = needCasting(
            ArgC, ArgC, FV->getInternalParam(PIdx), FV->getExternalParam(PIdx));
        if (CastKind != NO_CAST) {
          surroundByCast(FV->getExternalParam(PIdx), TypeVar, CastKind, A);
          ExprsWithCast.insert(ignoreCheckedCImplicit(A));
          break;
        }
      }
    }
    PIdx++;
  }

  // Cast on return. Be sure not to place casts when the result is not used,
  // otherwise an externally unsafe function whose result is not used would end
  // up with a bounds cast around it. hasPersistentConstraints is used to
  // determine if an expression is used because any expression that is
  // eventually assigned to a variable or passed as a function argument will
  // be cached in the persistent constraint set.
  if (Info.hasPersistentConstraints(CE, Context)) {
    CVarSet DestinationConstraints = CR.getExprConstraintVarsSet(CE);
    for (auto *DstC : DestinationConstraints) {
      // Order of ParameterC and ArgumentC is reversed from when inserting
      // parameter casts because assignment now goes from returned to its
      // local use.
      CastNeeded CastKind = needCasting(FV->getInternalReturn(),
                                        FV->getExternalReturn(), DstC, DstC);
      if (ExprsWithCast.find(CE) == ExprsWithCast.end() &&
          CastKind != NO_CAST) {
        surroundByCast(DstC, nullptr, CastKind, CE);
        ExprsWithCast.insert(ignoreCheckedCImplicit(CE));
        break;
      }
    }
  }
  return true;
}

CastPlacementVisitor::CastNeeded CastPlacementVisitor::needCasting(
    ConstraintVariable *SrcInt, ConstraintVariable *SrcExt,
    ConstraintVariable *DstInt, ConstraintVariable *DstExt) {
  Constraints &CS = Info.getConstraints();

  // In this case, the source is internally unchecked (i.e., it has an itype).
  // Typically, no casting is required, but a CheckedC bug means that we need to
  // insert a cast. https://github.com/microsoft/checkedc-clang/issues/614
  if (!SrcInt->isSolutionChecked(CS.getVariables()) &&
      SrcExt->isSolutionChecked(CS.getVariables()) &&
      DstExt->isSolutionChecked(CS.getVariables())) {
    if (auto *DstPVC = dyn_cast<PVConstraint>(DstExt)) {
      if (!DstPVC->getCvars().empty()) {
        ConstAtom *CA =
            Info.getConstraints().getAssignment(DstPVC->getCvars().at(0));
        if (isa<NTArrAtom>(CA))
          return CAST_NT_ARRAY;
      }
    }
  }

  // No casting is required if the source exactly matches either the
  // destinations itype or the destinations regular type.
  if (SrcExt->solutionEqualTo(CS, DstExt, false) ||
      SrcExt->solutionEqualTo(CS, DstInt, false) ||
      SrcInt->solutionEqualTo(CS, DstExt, false) ||
      SrcInt->solutionEqualTo(CS, DstInt, false))
    return NO_CAST;

  // As a special case, no casting is required when passing an unchecked pointer
  // to a function with an itype in the original source code. This case is
  // required to avoid adding casts when a function has an itype and is defined
  // in the file. Because the function is defined, the internal type can solve
  // to checked, causing to appear fully checked (without itype). This would
  // cause a bounds cast to be inserted on unchecked calls to the function.
  if (!SrcExt->isSolutionChecked(CS.getVariables()) &&
      !DstInt->isSolutionFullyChecked(CS.getVariables()) &&
      DstInt->srcHasItype())
    return NO_CAST;

  if (DstInt->isSolutionChecked(CS.getVariables()))
    return CAST_TO_CHECKED;

  return CAST_TO_WILD;
}

// Get the string representation of the cast required for the call. The return
// is a pair of strings: a prefix and suffix string that form the complete cast
// when placed around the expression being cast.
std::pair<std::string, std::string>
CastPlacementVisitor::getCastString(ConstraintVariable *Dst,
                                    ConstraintVariable *TypeVar,
                                    CastNeeded CastKind) {
  switch (CastKind) {
  case CAST_NT_ARRAY:
    return std::make_pair("((" +
                              Dst->mkString(Info.getConstraints(),
                                            MKSTRING_OPTS(EmitName = false)) +
                              ")",
                          ")");
  case CAST_TO_WILD:
    return std::make_pair("((" + Dst->getRewritableOriginalTy() + ")", ")");
  case CAST_TO_CHECKED: {
    // Needed as default to TypeVar branch below, reset otherwise.
    std::string Type = "_Ptr<";
    std::string Suffix = ")";
    if (const auto *DstPVC = dyn_cast<PVConstraint>(Dst)) {
      assert("Checked cast not to a pointer" && !DstPVC->getCvars().empty());
      ConstAtom *CA =
          Info.getConstraints().getAssignment(DstPVC->getCvars().at(0));

      // TODO: Writing an _Assume_bounds_cast to an array type requires
      // inserting the bounds for destination array. But the names used in src
      // and dest may be different, so we need more sophisticated code to
      // convert to local variable names. Use unknown bounds for now.
      if (isa<ArrAtom>(CA)) {
        Type = "_Array_ptr<";
        Suffix = ", bounds(unknown))";
      } else if (isa<NTArrAtom>(CA)) {
        Type = "_Nt_array_ptr<";
        Suffix = ", bounds(unknown))";
      }
    }
    // The destination's type may be generic, which would have an out-of-scope
    // type var, so use the already analysed local type var instead
    if (TypeVar != nullptr) {
      Type +=
          TypeVar->mkString(Info.getConstraints(),
                            MKSTRING_OPTS(EmitName = false, EmitPointee = true)) +
          ">";
    } else {
      Type = Dst->mkString(Info.getConstraints(), MKSTRING_OPTS(EmitName = false));
    }
    return std::make_pair("_Assume_bounds_cast<" + Type + ">(", Suffix);
  }
  default:
    llvm_unreachable("No casting needed");
  }
}

void CastPlacementVisitor::surroundByCast(ConstraintVariable *Dst,
                                          ConstraintVariable *TypeVar,
                                          CastNeeded CastKind, Expr *E) {
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(E, *Context);
  if (!canWrite(PSL.getFileName())) {
    // 3C has known bugs that can cause attempted cast insertion in
    // unwritable files in common use cases. Until they are fixed, report a
    // warning rather than letting the main "unwritable change" error trigger
    // later.
    reportCustomDiagnostic(
        Writer.getSourceMgr().getDiagnostics(),
        DiagnosticsEngine::Warning,
        "3C internal error: tried to insert a cast into an unwritable file "
        "(https://github.com/correctcomputation/checkedc-clang/issues/454)",
        E->getBeginLoc());
    return;
  }

  auto CastStrs = getCastString(Dst, TypeVar, CastKind);

  // If E is already a cast expression, we will try to rewrite the cast instead
  // of adding a new expression.
  if (isa<CStyleCastExpr>(E->IgnoreParens()) && CastKind == CAST_TO_WILD) {
    auto *CE = cast<CStyleCastExpr>(E->IgnoreParens());
    SourceRange CastTypeRange(CE->getLParenLoc(), CE->getRParenLoc());
    assert("Cast expected to start with '('" && !CastStrs.first.empty() &&
           CastStrs.first[0] == '(');
    std::string CastStr = CastStrs.first.substr(1);
    // FIXME: This rewriting is known to fail on the benchmark programs.
    //        https://github.com/correctcomputation/checkedc-clang/issues/444
    rewriteSourceRange(Writer, CastTypeRange, CastStr, false);
    updateRewriteStats(CastKind);
  } else {
    // First try to insert the cast prefix and suffix around the expression in
    // the source code.
    bool FrontRewritable = Writer.isRewritable(E->getBeginLoc());
    bool EndRewritable = Writer.isRewritable(E->getEndLoc());
    if (FrontRewritable && EndRewritable) {
      bool BFail = Writer.InsertTextBefore(E->getBeginLoc(), CastStrs.first);
      bool EFail = Writer.InsertTextAfterToken(E->getEndLoc(), CastStrs.second);
      updateRewriteStats(CastKind);
      assert("Locations were rewritable, fail should not be possible." &&
             !BFail && !EFail);
    } else {
      // Sometimes we can't insert the cast around the expression due to macros
      // getting in the way. In these cases, we can sometimes replace the entire
      // expression source with a new string containing the original expression
      // and the cast.
      auto CRA = CharSourceRange::getTokenRange(E->getSourceRange());
      auto NewCRA = clang::Lexer::makeFileCharRange(
          CRA, Context->getSourceManager(), Context->getLangOpts());
      std::string SrcText(clang::tooling::getText(CRA, *Context));
      // This doesn't always work either. We can't rewrite if the cast needs to
      // be placed fully inside a macro rather than around a macro or on an
      // argument to the macro.
      if (!SrcText.empty()) {
        rewriteSourceRange(Writer, NewCRA,
                           CastStrs.first + SrcText + CastStrs.second);
        updateRewriteStats(CastKind);
      } else {
        reportCastInsertionFailure(E, CastStrs.first + CastStrs.second);
      }
    }
  }
}

void CastPlacementVisitor::reportCastInsertionFailure(
    Expr *E, const std::string &CastStr) {
  // FIXME: This is a warning rather than an error so that a new benchmark
  //        failure is not introduced in Lua.
  //        github.com/correctcomputation/checkedc-clang/issues/439
  reportCustomDiagnostic(Context->getDiagnostics(),
                         DiagnosticsEngine::Warning,
                         "Unable to surround expression with cast.\n"
                         "Intended cast: \"%0\"",
                         E->getExprLoc())
      << Context->getSourceManager().getExpansionRange(E->getSourceRange())
      << CastStr;
}

void CastPlacementVisitor::updateRewriteStats(CastNeeded CastKind) {
  auto &PStats = Info.getPerfStats();
  switch (CastKind) {
  case CAST_NT_ARRAY:
    PStats.incrementNumCheckedCasts();
    break;
  case CAST_TO_WILD:
    PStats.incrementNumWildCasts();
    break;
  case CAST_TO_CHECKED:
    PStats.incrementNumAssumeBounds();
    break;
  default:
    llvm_unreachable("Unhandled cast.");
  }
}

bool CastLocatorVisitor::VisitCastExpr(CastExpr *C) {
  ExprsWithCast.insert(C);
  if (!isa<ImplicitCastExpr>(C)) {
    Expr *Sub = ignoreCheckedCImplicit(C->getSubExpr());
    ExprsWithCast.insert(Sub);
  }
  return true;
}
