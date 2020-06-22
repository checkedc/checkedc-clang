//=--AVarBoundsInfo.cpp-------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of methods in AVarBoundsInfo.h.
//
//===----------------------------------------------------------------------===//

#include "clang/CConv/AVarBoundsInfo.h"

bool AVarBoundsInfo::isValidBoundVariable(clang::Decl *D) {
  if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    return !VD->getNameAsString().empty();
  }
  if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
    return !PD->getNameAsString().empty();
  }
  if(FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
    return !FD->getNameAsString().empty();
  }
  return false;
}

void AVarBoundsInfo::insertBounds(clang::Decl *D, ABounds *B) {
  assert(isValidBoundVariable(D) && "Declaration not a valid bounds variable");
  BoundsKey BK;
  getVariable(D, BK);
  // If there is already bounds information, release it.
  if (BInfo.find(BK) != BInfo.end()) {
    delete (BInfo[BK]);
  }
  BInfo[BK] = B;
}

bool AVarBoundsInfo::getVariable(clang::Decl *D, BoundsKey &R) {
  if (isValidBoundVariable(D)) {
    if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
      R = getVariable(VD);
    }
    if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
      R = getVariable(PD);
    }
    if (FieldDecl *FD = dyn_cast<FieldDecl>(D)) {
      R = getVariable(FD);
    }
    return true;
  }
  return false;
}

void AVarBoundsInfo::insertVariable(clang::Decl *D) {
  BoundsKey Tmp;
  getVariable(D, Tmp);
}

BoundsKey AVarBoundsInfo::getVariable(clang::VarDecl *VD) {
  assert(isValidBoundVariable(VD) && "Not a valid bound declaration.");
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(VD, VD->getASTContext());
  if (!hasVarKey(PSL)) {
    BoundsKey NK = ++BCount;
    insertVarKey(PSL, NK);
    ProgramVarScope *PVS = nullptr;
    if (VD->hasGlobalStorage()) {
      PVS = GlobalScope::getGlobalScope();
    } else {
      FunctionDecl *FD =
          dyn_cast<FunctionDecl>(VD->getParentFunctionOrMethod());
      if (FD != nullptr) {
        PVS = FunctionScope::getFunctionScope(FD->getNameAsString(),
                                              FD->isStatic());
      }
    }
    assert(PVS != nullptr && "Context not null");
    auto *PVar = new ProgramVar(NK, VD->getNameAsString(), PVS);
    insertProgramVar(NK, PVar);
  }
  return getVarKey(PSL);
}

BoundsKey AVarBoundsInfo::getVariable(clang::ParmVarDecl *PVD) {
  assert(isValidBoundVariable(PVD) && "Not a valid bound declaration.");
  FunctionDecl *FD = dyn_cast<FunctionDecl>(PVD->getDeclContext());
  int ParamIdx = -1;
  // Get parameter index.
  for (unsigned i=0; i<FD->getNumParams(); i++) {
    if (FD->getParamDecl(i) == PVD) {
      ParamIdx = i;
      break;
    }
  }

  auto ParamKey = std::make_tuple(FD->getNameAsString(),
                                  FD->isStatic(), ParamIdx);
  assert(ParamIdx >= 0 && "Unable to find parameter.");
  if (ParamDeclVarMap.find(ParamKey) == ParamDeclVarMap.end()) {
    BoundsKey NK = ++BCount;
    FunctionScope *FS =
        FunctionScope::getFunctionScope(FD->getNameAsString(), FD->isStatic());
    auto *PVar = new ProgramVar(NK, PVD->getNameAsString(), FS);
    insertProgramVar(NK, PVar);
    ParamDeclVarMap[ParamKey] = NK;
  }
  return ParamDeclVarMap[ParamKey];
}

BoundsKey AVarBoundsInfo::getVariable(clang::FieldDecl *FD) {
  assert(isValidBoundVariable(FD) && "Not a valid bound declaration.");
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(FD, FD->getASTContext());
  if (!hasVarKey(PSL)) {
    BoundsKey NK = ++BCount;
    insertVarKey(PSL, NK);
    std::string StName = FD->getParent()->getNameAsString();
    StructScope *SS = StructScope::getStructScope(StName);
    auto *PVar = new ProgramVar(NK, FD->getNameAsString(), SS);
    insertProgramVar(NK, PVar);
  }
  return getVarKey(PSL);
}

bool AVarBoundsInfo::getVariable(clang::Expr *E,
                                 const ASTContext &C,
                                 BoundsKey &Res) {
  llvm::APSInt ConsVal;
  bool Ret = false;
  E = E->IgnoreParenCasts();
  if (E->isIntegerConstantExpr(ConsVal, C)) {
    Res = getVarKey(ConsVal);
    Ret = true;
  } else if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
    auto *D = DRE->getDecl();
    Ret = getVariable(D, Res);
    if (!Ret) {
      assert(false && "Invalid declaration found inside bounds expression");
    }
  } else {
    // assert(false && "Variable inside bounds declaration is an expression");
  }
  return Ret;
}

bool AVarBoundsInfo::addAssignment(clang::Decl *L, clang::Decl *R) {
  BoundsKey BL, BR;
  if (getVariable(L, BL) && getVariable(R, BR)) {
    return addAssignment(BL, BR);
  }
  return false;
}

bool AVarBoundsInfo::addAssignment(clang::DeclRefExpr *L,
                                   clang::DeclRefExpr *R) {
  return addAssignment(L->getDecl(), R->getDecl());
}

bool AVarBoundsInfo::addAssignment(BoundsKey L, BoundsKey R) {
  ProgVarGraph.addEdge(L, R);
  return true;
}

ProgramVar *AVarBoundsInfo::getProgramVar(BoundsKey VK) {
  ProgramVar *Ret = nullptr;
  if (PVarInfo.find(VK) != PVarInfo.end()) {
    Ret = PVarInfo[VK];
  }
  return Ret;
}

bool AVarBoundsInfo::hasVarKey(PersistentSourceLoc &PSL) {
  return DeclVarMap.find(PSL) != DeclVarMap.end();
}

BoundsKey AVarBoundsInfo::getVarKey(PersistentSourceLoc &PSL) {
  assert (hasVarKey(PSL) && "VarKey doesn't exist");
  return DeclVarMap[PSL];
}

BoundsKey AVarBoundsInfo::getVarKey(llvm::APSInt &API) {
  if (ConstVarKeys.find(API) == ConstVarKeys.end()) {
    BoundsKey NK = ++BCount;
    ConstVarKeys[API] = NK;
    std::string ConsString = std::to_string(API.abs().getZExtValue());
    ProgramVar *NPV = new ProgramVar(NK, ConsString,
                                     GlobalScope::getGlobalScope(), true);
    insertProgramVar(NK, NPV);
  }
  return ConstVarKeys[API];
}

void AVarBoundsInfo::insertVarKey(PersistentSourceLoc &PSL, BoundsKey NK) {
  DeclVarMap[PSL] = NK;
}

void AVarBoundsInfo::insertProgramVar(BoundsKey NK, ProgramVar *PV) {
  if (getProgramVar(NK) != nullptr) {
    // Free the already created variable.
    auto *E = PVarInfo[NK];
    delete (E);
  }
  PVarInfo[NK] = PV;
}