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

void AVarBoundsInfo::insertDeclaredBounds(clang::Decl *D, ABounds *B) {
  assert(isValidBoundVariable(D) && "Declaration not a valid bounds variable");
  BoundsKey BK;
  tryGetVariable(D, BK);
  if (B != nullptr) {
    // If there is already bounds information, release it.
    if (BInfo.find(BK) != BInfo.end()) {
      delete (BInfo[BK]);
    }
    BInfo[BK] = B;
  } else {
    // Set bounds to be invalid.
    InvalidBounds.insert(BK);
  }
}

bool AVarBoundsInfo::tryGetVariable(clang::Decl *D, BoundsKey &R) {
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

bool AVarBoundsInfo::tryGetVariable(clang::Expr *E,
                                    const ASTContext &C,
                                    BoundsKey &Res) {
  llvm::APSInt ConsVal;
  bool Ret = false;
  if (E != nullptr) {
    E = E->IgnoreParenCasts();
    if (E->getType()->isArithmeticType() &&
        E->isIntegerConstantExpr(ConsVal, C)) {
      Res = getVarKey(ConsVal);
      Ret = true;
    } else if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
      auto *D = DRE->getDecl();
      Ret = tryGetVariable(D, Res);
      if (!Ret) {
        assert(false && "Invalid declaration found inside bounds expression");
      }
    } else {
      // assert(false && "Variable inside bounds declaration is an expression");
    }
  }
  return Ret;
}

bool AVarBoundsInfo::mergeBounds(BoundsKey L, ABounds *B) {
  bool RetVal = false;
  if (BInfo.find(L) != BInfo.end()) {
    // If previous computed bounds are not same? Then release the old bounds.
    if (!BInfo[L]->areSame(B)) {
      InvalidBounds.insert(L);
      delete (BInfo[L]);
      BInfo.erase(L);
    }
  } else {
    BInfo[L] = B;
    RetVal = true;
  }
  return RetVal;
}

bool AVarBoundsInfo::removeBounds(BoundsKey L) {
  bool RetVal = false;
  if (BInfo.find(L) != BInfo.end()) {
    delete (BInfo[L]);
    BInfo.erase(L);
    RetVal = true;
  }
  return RetVal;
}

bool AVarBoundsInfo::replaceBounds(BoundsKey L, ABounds *B) {
  removeBounds(L);
  return mergeBounds(L, B);
}

ABounds *AVarBoundsInfo::getBounds(BoundsKey L) {
  if (InvalidBounds.find(L) == InvalidBounds.end() &&
      BInfo.find(L) != BInfo.end()) {
    return BInfo[L];
  }
  return nullptr;
}

void AVarBoundsInfo::insertVariable(clang::Decl *D) {
  BoundsKey Tmp;
  tryGetVariable(D, Tmp);
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
    if (VD->getType()->isPointerType())
      PointerBoundsKey.insert(NK);
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
  if (ParamDeclVarMap.left.find(ParamKey) == ParamDeclVarMap.left.end()) {
    BoundsKey NK = ++BCount;
    FunctionParamScope *FPS =
        FunctionParamScope::getFunctionParamScope(FD->getNameAsString(),
                                                  FD->isStatic());
    auto *PVar = new ProgramVar(NK, PVD->getNameAsString(), FPS);
    insertProgramVar(NK, PVar);
    ParamDeclVarMap.insert(ParmMapItemType(ParamKey, NK));
    if (PVD->getType()->isPointerType())
      PointerBoundsKey.insert(NK);
  }
  return ParamDeclVarMap.left.at(ParamKey);
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
    if (FD->getType()->isPointerType())
      PointerBoundsKey.insert(NK);
  }
  return getVarKey(PSL);
}

bool AVarBoundsInfo::addAssignment(clang::Decl *L, clang::Decl *R) {
  BoundsKey BL, BR;
  if (tryGetVariable(L, BL) && tryGetVariable(R, BR)) {
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
  return DeclVarMap.left.find(PSL) != DeclVarMap.left.end();
}

BoundsKey AVarBoundsInfo::getVarKey(PersistentSourceLoc &PSL) {
  assert (hasVarKey(PSL) && "VarKey doesn't exist");
  return DeclVarMap.left.at(PSL);
}

BoundsKey AVarBoundsInfo::getConstKey(uint64_t value) {
  if (ConstVarKeys.find(value) == ConstVarKeys.end()) {
    BoundsKey NK = ++BCount;
    ConstVarKeys[value] = NK;
    std::string ConsString = std::to_string(value);
    ProgramVar *NPV = new ProgramVar(NK, ConsString,
                                     GlobalScope::getGlobalScope(), true);
    insertProgramVar(NK, NPV);
  }
  return ConstVarKeys[value];
}

BoundsKey AVarBoundsInfo::getVarKey(llvm::APSInt &API) {
  return getConstKey(API.abs().getZExtValue());
}

void AVarBoundsInfo::insertVarKey(PersistentSourceLoc &PSL, BoundsKey NK) {
  DeclVarMap.insert(DeclMapItemType(PSL, NK));
}

void AVarBoundsInfo::insertProgramVar(BoundsKey NK, ProgramVar *PV) {
  if (getProgramVar(NK) != nullptr) {
    // Free the already created variable.
    auto *E = PVarInfo[NK];
    delete (E);
  }
  PVarInfo[NK] = PV;
}