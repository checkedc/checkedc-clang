//=--CheckedRegions.cpp-------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains implementation of the functions and
// classes of CheckedRegions.h
//===----------------------------------------------------------------------===//

#include "clang/3C/CheckedRegions.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ArrayBoundsInferenceConsumer.h"
#include "clang/3C/ConstraintResolver.h"
#include "clang/3C/MappingVisitor.h"
#include "clang/3C/RewriteUtils.h"
#include "clang/3C/Utils.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Tooling/Refactoring/SourceCode.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <sstream>

using namespace llvm;
using namespace clang;

// CheckedRegionAdder

bool CheckedRegionAdder::VisitCompoundStmt(CompoundStmt *S) {
  llvm::FoldingSetNodeID Id;
  ast_type_traits::DynTypedNode DTN = ast_type_traits::DynTypedNode::create(*S);

  S->Profile(Id, *Context, true);
  switch (Map[Id]) {
  case IS_UNCHECKED:
    if (isParentChecked(DTN) && !isFunctionBody(S)) {
      auto Loc = S->getBeginLoc();
      Writer.InsertTextBefore(Loc, "_Unchecked ");
    }
    break;
  case IS_CHECKED:
    if (!isParentChecked(DTN)) {
      auto Loc = S->getBeginLoc();
      Writer.InsertTextBefore(Loc, "_Checked ");
    }
    break;
  default:
    llvm_unreachable("Bad flag in CheckedRegionAdder");
  }

  return true;
}

bool CheckedRegionAdder::VisitCallExpr(CallExpr *C) {
  auto *FD = C->getDirectCallee();
  FoldingSetNodeID ID;
  ast_type_traits::DynTypedNode DTN = ast_type_traits::DynTypedNode::create(*C);

  C->Profile(ID, *Context, true);
  if (FD && FD->isVariadic() && Map[ID] == IS_CONTAINED &&
      isParentChecked(DTN)) {
    auto Begin = C->getBeginLoc();
    Writer.InsertTextBefore(Begin, "_Unchecked { ");
    auto End = C->getEndLoc();
    Writer.InsertTextAfterToken(End, "; }");
  }

  return true;
}

typedef std::pair<const CompoundStmt *, int> StmtPair;

StmtPair
CheckedRegionAdder::findParentCompound(const ast_type_traits::DynTypedNode &N,
                                       int Distance = 1) {
  auto Parents = Context->getParents(N);
  if (Parents.empty())
    return std::make_pair(nullptr, INT_MAX);
  std::vector<StmtPair> Results;
  Results.reserve(Parents.size());
  for (auto Parent : Parents)
    if (const auto *S = Parent.get<CompoundStmt>())
      Results.push_back(std::make_pair(S, Distance));
    else
      Results.push_back(findParentCompound(Parent, Distance + 1));

  auto Min = min_element(
      Results.begin(), Results.end(),
      [](const StmtPair &A, const StmtPair &B) { return A.second < B.second; });
  return *Min;
}

bool CheckedRegionAdder::isFunctionBody(CompoundStmt *S) {
  const auto &Parents = Context->getParents(*S);
  if (Parents.empty()) {
    return false;
  }
  return Parents[0].get<FunctionDecl>();
}

bool CheckedRegionAdder::isParentChecked(
    const ast_type_traits::DynTypedNode &DTN) {
  if (const auto *Parent = findParentCompound(DTN).first) {
    llvm::FoldingSetNodeID ID;
    Parent->Profile(ID, *Context, true);
    return Map[ID] == IS_CHECKED || isWrittenChecked(Parent);
  }
  return false;
}

bool CheckedRegionAdder::isWrittenChecked(const clang::CompoundStmt *S) {
  CheckedScopeSpecifier WCSS = S->getWrittenCheckedSpecifier();
  switch (WCSS) {
  case CSS_None:
    return false;
  case CSS_Unchecked:
    return false;
  case CSS_Bounds:
    return true;
  case CSS_Memory:
    return true;
  }
  llvm_unreachable("Invalid Checked Scope Specifier.");
}

// CheckedRegionFinder

bool CheckedRegionFinder::VisitForStmt(ForStmt *S) {
  handleChildren(S->children());
  return false;
}

bool CheckedRegionFinder::VisitSwitchStmt(SwitchStmt *S) {
  handleChildren(S->children());
  return false;
}

bool CheckedRegionFinder::VisitIfStmt(IfStmt *S) {
  handleChildren(S->children());
  return false;
}

bool CheckedRegionFinder::VisitWhileStmt(WhileStmt *S) {
  handleChildren(S->children());
  return false;
}

bool CheckedRegionFinder::VisitDoStmt(DoStmt *S) {
  handleChildren(S->children());
  return false;
}

bool CheckedRegionFinder::VisitCompoundStmt(CompoundStmt *S) {
  // Visit all subblocks, find all unchecked types
  bool Localwild = 0;
  for (const auto &SubStmt : S->children()) {
    CheckedRegionFinder Sub(Context, Writer, Info, Seen, Map, EmitWarnings);
    Sub.TraverseStmt(SubStmt);
    Localwild |= Sub.Wild;
  }

  markChecked(S, Localwild);

  Wild = false;

  llvm::FoldingSetNodeID Id;
  S->Profile(Id, *Context, true);
  Seen.insert(Id);

  // Compound Statements should be the bottom of the visitor,
  // as it creates it's own sub-visitor.
  return false;
}

bool CheckedRegionFinder::VisitStmtExpr(StmtExpr *SE) {
  Wild = true;
  return false;
}

bool CheckedRegionFinder::VisitCStyleCastExpr(CStyleCastExpr *E) {
  // TODO This is over cautious
  Wild = true;
  return true;
}

bool CheckedRegionFinder::VisitCallExpr(CallExpr *C) {
  auto *FD = C->getDirectCallee();
  FoldingSetNodeID ID;
  C->Profile(ID, *Context, true);
  if (FD && FD->isVariadic()) {
    Wild = !isInStatementPosition(C);
    Map[ID] = isInStatementPosition(C) ? IS_CONTAINED : IS_UNCHECKED;
  } else {
    if (FD) {
      auto Type = FD->getReturnType();
      Wild |=
          (!(FD->hasPrototype() || FD->doesThisDeclarationHaveABody())) ||
          containsUncheckedPtr(Type) ||
          (std::any_of(FD->param_begin(), FD->param_end(), [this](Decl *Param) {
            CVarOption CV = Info.getVariable(Param, Context);
            return isWild(CV);
          }));
    }
    handleChildren(C->children());
    Map[ID] = Wild ? IS_UNCHECKED : IS_CHECKED;
  }

  return false;
}

bool CheckedRegionFinder::VisitVarDecl(VarDecl *VD) {
  CVarOption CV = Info.getVariable(VD, Context);
  Wild = isWild(CV) || containsUncheckedPtr(VD->getType());
  return true;
}

bool CheckedRegionFinder::VisitParmVarDecl(ParmVarDecl *PVD) {
  // Check if the variable is WILD.
  CVarOption CV = Info.getVariable(PVD, Context);
  Wild |= isWild(CV) || containsUncheckedPtr(PVD->getType());
  return true;
}

bool CheckedRegionFinder::VisitMemberExpr(MemberExpr *E) {
  ValueDecl *VD = E->getMemberDecl();
  if (VD) {
    // Check if the variable is WILD.
    CVarOption Cv = Info.getVariable(VD, Context);
    if (Cv.hasValue() &&
        Cv.getValue().hasWild(Info.getConstraints().getVariables()))
      Wild = true;
    // Check if the variable contains unchecked types.
    Wild |= containsUncheckedPtr(VD->getType());
  }
  return true;
}

bool CheckedRegionFinder::VisitDeclRefExpr(DeclRefExpr *DR) {
  auto T = DR->getType();
  auto *D = DR->getDecl();
  CVarOption CV = Info.getVariable(D, Context);
  bool IW = isWild(CV) || containsUncheckedPtr(T);

  if (auto *FD = dyn_cast<FunctionDecl>(D)) {
    auto *FV = Info.getFuncConstraint(FD, Context);
    IW |= FV->hasWild(Info.getConstraints().getVariables());
    for (const auto &Param : FD->parameters()) {
      CVarOption CV = Info.getVariable(Param, Context);
      IW |= isWild(CV);
    }
  }

  Wild |= IW;
  return true;
}

void CheckedRegionFinder::handleChildren(const Stmt::child_range &Stmts) {
  for (const auto &SubStmt : Stmts) {
    CheckedRegionFinder Sub(Context, Writer, Info, Seen, Map, EmitWarnings);
    Sub.TraverseStmt(SubStmt);
    Wild |= Sub.Wild;
  }
}

// Check if this compound statement is the body
// to a function with unsafe parameters.
bool CheckedRegionFinder::hasUncheckedParameters(CompoundStmt *S) {
  const auto &Parents = Context->getParents(*S);
  if (Parents.empty()) {
    return false;
  }

  const auto *Parent = Parents[0].get<FunctionDecl>();
  if (!Parent) {
    return false;
  }

  int Localwild = false;
  for (auto *Child : Parent->parameters()) {
    CheckedRegionFinder Sub(Context, Writer, Info, Seen, Map, EmitWarnings);
    Sub.TraverseParmVarDecl(Child);
    Localwild |= Sub.Wild;
  }

  return Localwild || Parent->isVariadic();
}

bool CheckedRegionFinder::isInStatementPosition(CallExpr *C) {
  // First check if our parent is a compound statement
  const auto &Parents = Context->getParents(*C);
  if (Parents.empty()) {
    return false; // This case shouldn't happen,
                  // but if it does play it safe and mark WILD.
  }
  const auto *Parent = Parents[0].get<CompoundStmt>();
  if (Parent) {
    //Check if we are the only child
    auto Childs = Parent->children();
    int NumChilds = std::distance(Childs.begin(), Childs.end());
    return NumChilds > 1;
  }
  //TODO there are other statement positions
  //     besides child of compound stmt
  auto PSL = PersistentSourceLoc::mkPSL(C, *Context);
  emitCauseDiagnostic(&PSL);
  return false;
}

bool CheckedRegionFinder::isWild(CVarOption Cv) {
  if (Cv.hasValue() &&
      Cv.getValue().hasWild(Info.getConstraints().getVariables()))
    return true;
  return false;
}

bool CheckedRegionFinder::containsUncheckedPtr(QualType Qt) {
  // TODO does a more efficient representation exist?
  std::set<std::string> Seen;
  return containsUncheckedPtrAcc(Qt, Seen);
}

// Recursively determine if a type is unchecked.
bool CheckedRegionFinder::containsUncheckedPtrAcc(QualType Qt,
                                                  std::set<std::string> &Seen) {
  auto Ct = Qt.getCanonicalType();
  auto TyStr = Ct.getAsString();
  bool IsSeen = false;
  auto Search = Seen.find(TyStr);
  if (Search == Seen.end()) {
    Seen.insert(TyStr);
  } else {
    IsSeen = true;
  }

  if (Ct->isFunctionPointerType()) {
    if (const auto *FPT = dyn_cast<FunctionProtoType>(Ct->getPointeeType())) {
      auto PTs = FPT->getParamTypes();
      bool Params =
          std::any_of(PTs.begin(), PTs.end(), [this, &Seen](QualType QT) {
            return containsUncheckedPtrAcc(QT, Seen);
          });
      return containsUncheckedPtrAcc(FPT->getReturnType(), Seen) || Params;
    }
    return false;
  }
  if (Ct->isVoidPointerType()) {
    return true;
  }
  if (Ct->isVoidType()) {
    return true;
  }
  if (Ct->isPointerType()) {
    return containsUncheckedPtrAcc(Ct->getPointeeType(), Seen);
  }
  if (Ct->isRecordType()) {
    if (IsSeen) {
      return false;
    }
    return isUncheckedStruct(Ct, Seen);
  }
  return false;
}

// Iterate through all fields of the struct and find unchecked types.
bool CheckedRegionFinder::isUncheckedStruct(QualType Qt,
                                            std::set<std::string> &Seen) {
  const auto *RcdTy = dyn_cast<RecordType>(Qt);
  if (RcdTy) {
    auto *D = RcdTy->getDecl();
    if (D) {
      bool Unsafe = false;
      for (auto const &Fld : D->fields()) {
        auto Ftype = Fld->getType();
        Unsafe |= containsUncheckedPtrAcc(Ftype, Seen);
        CVarOption Cv = Info.getVariable(Fld, Context);
        Unsafe |= (Cv.hasValue() &&
                   Cv.getValue().hasWild(Info.getConstraints().getVariables()));
      }
      return Unsafe;
    }
  }

  return false;
}

// Mark the given compound statement with
// whether or not it is checked
void CheckedRegionFinder::markChecked(CompoundStmt *S, int Localwild) {
  auto Cur = S->getWrittenCheckedSpecifier();
  llvm::FoldingSetNodeID Id;
  S->Profile(Id, *Context, true);

  bool IsChecked = !hasUncheckedParameters(S) &&
                   Cur == CheckedScopeSpecifier::CSS_None && Localwild == 0;

  Map[Id] = IsChecked ? IS_CHECKED : IS_UNCHECKED;
}

void CheckedRegionFinder::emitCauseDiagnostic(PersistentSourceLoc *PSL) {
  if (Emitted.find(PSL) == Emitted.end()) {
    clang::DiagnosticsEngine &DE = Context->getDiagnostics();
    unsigned ID =
        DE.getCustomDiagID(DiagnosticsEngine::Warning,
                           "Root cause of unchecked region: Variadic Call");
    SourceManager &SM = Context->getSourceManager();
    const auto *File = SM.getFileManager().getFile(PSL->getFileName());
    SourceLocation SL =
        SM.translateFileLineCol(File, PSL->getLineNo(), PSL->getColSNo());
    if (SL.isValid())
      DE.Report(SL, ID);
    Emitted.insert(PSL);
  }
}
