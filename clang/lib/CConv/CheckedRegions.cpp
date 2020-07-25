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

#include "clang/CConv/ConstraintResolver.h"
#include "clang/CConv/RewriteUtils.h"
#include "clang/CConv/Utils.h"
#include "clang/CConv/CheckedRegions.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CConv/ArrayBoundsInferenceConsumer.h"
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/MappingVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Tooling/Refactoring/SourceCode.h"
#include <sstream>

using namespace llvm;
using namespace clang;

// CheckedRegionAdder

bool CheckedRegionAdder::VisitCompoundStmt(CompoundStmt *S) { 
  llvm::FoldingSetNodeID Id;
  S->Profile(Id, *Context, true);
  switch (Map[Id]) {
    case IS_UNCHECKED: return true;
    case IS_CHECKED:   
                        auto Loc = S->getBeginLoc();
                        Writer.InsertTextBefore(Loc, "_Checked ");
                        return false;
  }

  llvm_unreachable("Bad flag in CheckedRegionAdder");
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

void CheckedRegionFinder::handleChildren(const Stmt::child_range &Stmts) { 
  int Localwild = 0;

  for (const auto &SubStmt : Stmts) {
    CheckedRegionFinder Sub(Context, Writer, Info, Seen, Map);
    Sub.TraverseStmt(SubStmt);
    Localwild += Sub.Nwild;
  }
  Nwild += Localwild;
}



bool CheckedRegionFinder::VisitCompoundStmt(CompoundStmt *S) {
  // Visit all subblocks, find all unchecked types
  int Localwild = 0;
  for (const auto &SubStmt : S->children()) {
    CheckedRegionFinder Sub(Context,Writer,Info,Seen,Map);
    Sub.TraverseStmt(SubStmt);
    Localwild += Sub.Nwild;
    Nchecked += Sub.Nchecked;
    Ndecls += Sub.Ndecls;
  }

  addUncheckedAnnotation(S, Localwild);

  // Compound Statements are always considered to have 0 wild types
  // This is because a compound statement marked w/ _Unchecked can live
  // inside a _Checked region.
  Nwild = 0;

  llvm::FoldingSetNodeID Id;
  S->Profile(Id, *Context, true);
  Seen.insert(Id);

  // Compound Statements should be the bottom of the visitor,
  // as it creates it's own sub-visitor.
  return false;
}

bool CheckedRegionFinder::VisitCStyleCastExpr(CStyleCastExpr *E) {
  // TODO This is over cautious
  Nwild++;
  return true;
}

// Check if this compound statement is the body
// to a function with unsafe parameters.
bool CheckedRegionFinder::hasUncheckedParameters(CompoundStmt *S) {
  const auto &Parents = Context->getParents(*S);
  if (Parents.empty()) {
    return false;
  }

  auto Parent = Parents[0].get<FunctionDecl>();
  if (!Parent) {
    return false;
  }

  int Localwild = 0;
  for (auto Child : Parent->parameters()) {
    CheckedRegionFinder Sub(Context,Writer,Info,Seen,Map);
    Sub.TraverseParmVarDecl(Child);
    Localwild += Sub.Nwild;
  }

  return Localwild != 0 || Parent->isVariadic();
}


bool CheckedRegionFinder::VisitUnaryOperator(UnaryOperator *U) {
  //TODO handle computing pointers
  if (U->getOpcode() == UO_AddrOf) {
    // wild++;
  }
  return true;
}

bool CheckedRegionFinder::isInStatementPosition(CallExpr *C) {
  // First check if our parent is a compound statement
  const auto &Parents = Context->getParents(*C);
  if (Parents.empty()) {
    return false; // This case shouldn't happen,
                  // but if it does play it safe and mark WILD.
  }
  auto Parent = Parents[0].get<CompoundStmt>();
  if (Parent) {
    //Check if we are the only child
    int NumChilds = 0;
    for (const auto &child : Parent->children()) {
      NumChilds++;
    }
    return NumChilds > 1;
  } else {
    //TODO there are other statement positions
    //     besides child of compound stmt
    return false;
  }
}

bool CheckedRegionFinder::VisitCallExpr(CallExpr *C) {
  auto FD = C->getDirectCallee();
  if (FD && FD->isVariadic()) {
    // If this variadic call is in statement positon, we can wrap in it
    // an unsafe block and avoid polluting the entire block as unsafe.
    // If it's not (as in it is used in an expression) then we fall back to
    // reporting an WILD value.
    if (isInStatementPosition(C)) {
      // Insert an _Unchecked block around the call
      auto Begin = C->getBeginLoc();
      Writer.InsertTextBefore(Begin, "_Unchecked { ");
      auto End = C->getEndLoc();
      Writer.InsertTextAfterToken(End, "; }");
    } else {
      // Call is inside an epxression, mark WILD.
      Nwild++;
    }
  }
  if (FD) {
    auto type = FD->getReturnType();
    if (type->isPointerType())
      Nwild++;
  }
  return true;
}


bool CheckedRegionFinder::VisitVarDecl(VarDecl *VD) {
  // Check if the variable is WILD.
  bool FoundWild = false;
  std::set<ConstraintVariable *> CVSet = Info.getVariable(VD, Context);
  for (auto Cv : CVSet) {
    if (Cv->hasWild(Info.getConstraints().getVariables())) {
      FoundWild = true;
    }
  }

  if (FoundWild)
    Nwild++;


  // Check if the variable contains an unchecked type.
  if (isUncheckedPtr(VD->getType()))
    Nwild++;
  Ndecls++;
  return true;
}

bool CheckedRegionFinder::VisitParmVarDecl(ParmVarDecl *PVD) {
  // Check if the variable is WILD.
  bool FoundWild = false;
  std::set<ConstraintVariable *> CVSet = Info.getVariable(PVD, Context);
  for (auto Cv : CVSet) {
    if (Cv->hasWild(Info.getConstraints().getVariables())) {
      FoundWild = true;
    }
  }

  if (FoundWild)
    Nwild++;

  // Check if the variable is a void*.
  if (isUncheckedPtr(PVD->getType()))
    Nwild++;
  Ndecls++;
  return true;
}

bool CheckedRegionFinder::isUncheckedPtr(QualType Qt) {
  // TODO does a more efficient representation exist?
  std::set<std::string> Seen;
  return isUncheckedPtrAcc(Qt, Seen);
}

// Recursively determine if a type is unchecked.
bool CheckedRegionFinder::isUncheckedPtrAcc(QualType Qt, std::set<std::string> &Seen) {
  auto Ct = Qt.getCanonicalType();
  auto TyStr = Ct.getAsString();
  auto Search = Seen.find(TyStr);
  if (Search == Seen.end()) {
    Seen.insert(TyStr);
  } else {
    return false;
  }

  if (Ct->isVoidPointerType()) {
    return true;
  } else if (Ct->isVoidType()) {
    return true;
  } if (Ct->isPointerType()) {
    return isUncheckedPtrAcc(Ct->getPointeeType(), Seen);
  } else if (Ct->isRecordType()) {
    return isUncheckedStruct(Ct, Seen);
  } else {
    return false;
  }
}

// Iterate through all fields of the struct and find unchecked types.
// TODO doesn't handle recursive structs correctly
bool CheckedRegionFinder::isUncheckedStruct(QualType Qt, std::set<std::string> &Seen) {
  auto RcdTy = dyn_cast<RecordType>(Qt);
  if (RcdTy) {
    auto D = RcdTy->getDecl();
    if (D) {
      bool Unsafe = false;
      for (auto const &Fld : D->fields()) {
        auto Ftype = Fld->getType();
        Unsafe |= isUncheckedPtrAcc(Ftype, Seen);
        std::set<ConstraintVariable *> CVSet =
            Info.getVariable(Fld, Context);
        for (auto Cv : CVSet) {
          Unsafe |= Cv->hasWild(Info.getConstraints().getVariables());
        }
      }
      return Unsafe;
    } else {
      return true;
    }
  } else {
    return false;
  }
}

void CheckedRegionFinder::addUncheckedAnnotation(CompoundStmt *S, int Localwild) {
  auto Cur = S->getWrittenCheckedSpecifier();

  llvm::FoldingSetNodeID Id;
  S->Profile(Id, *Context, true);
  auto Search = Seen.find(Id);

  if (Search == Seen.end()) {
    auto Loc = S->getBeginLoc();
    bool IsChecked = !hasUncheckedParameters(S) &&
                   Cur == CheckedScopeSpecifier::CSS_None && Localwild == 0;

    Map[Id] = IsChecked ? IS_CHECKED : IS_UNCHECKED;

    // Don't add _Unchecked to top level functions.
    if ((!IsChecked && !isFunctionBody(S))) {
      Writer.InsertTextBefore(Loc, "_Unchecked ");
    }
  }
}

bool CheckedRegionFinder::isFunctionBody(CompoundStmt *S) {
  const auto &Parents = Context->getParents(*S);
  if (Parents.empty()) {
    return false;
  }
  return Parents[0].get<FunctionDecl>();
}


bool CheckedRegionFinder::VisitMemberExpr(MemberExpr *E){
  ValueDecl *VD = E->getMemberDecl();
  if (VD) {
    // Check if the variable is WILD.
    bool FoundWild = false;
    std::set<ConstraintVariable *> CVSet = Info.getVariable(VD, Context);
    for (auto Cv : CVSet) {
      if (Cv->hasWild(Info.getConstraints().getVariables())) {
        FoundWild = true;
      }
    }

    if (FoundWild)
      Nwild++;

    // Check if the variable is a void*.
    if (isUncheckedPtr(VD->getType()))
      Nwild++;
    Ndecls++;
  }
  return true;
}
