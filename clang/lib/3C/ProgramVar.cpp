//=--ProgramVar.cpp-----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains implementation of methods in ProgramVar.h
//
//===----------------------------------------------------------------------===//

#include "clang/3C/ProgramVar.h"

GlobalScope *GlobalScope::ProgScope = nullptr;
std::set<StructScope, PVSComp> StructScope::AllStScopes;
std::set<FunctionParamScope, PVSComp> FunctionParamScope::AllFnParamScopes;
std::set<FunctionScope, PVSComp> FunctionScope::AllFnScopes;
std::set<CtxFunctionArgScope, PVSComp> CtxFunctionArgScope::AllCtxFnArgScopes;

GlobalScope *GlobalScope::getGlobalScope() {
  if (ProgScope == nullptr) {
    ProgScope = new GlobalScope();
  }
  return ProgScope;
}

const StructScope *StructScope::getStructScope(std::string StName) {
  StructScope TmpS(StName);
  if (AllStScopes.find(TmpS) == AllStScopes.end()) {
    AllStScopes.insert(TmpS);
  }
  const auto &SS = *AllStScopes.find(TmpS);
  return &SS;
}

const FunctionParamScope *
FunctionParamScope::getFunctionParamScope(std::string FnName, bool IsSt) {
  FunctionParamScope TmpFPS(FnName, IsSt);
  if (AllFnParamScopes.find(TmpFPS) == AllFnParamScopes.end()) {
    AllFnParamScopes.insert(TmpFPS);
  }
  const auto &FPS = *AllFnParamScopes.find(TmpFPS);
  return &FPS;
}

const CtxFunctionArgScope *
CtxFunctionArgScope::getCtxFunctionParamScope(const FunctionParamScope *FPS,
                                              const PersistentSourceLoc &PSL) {
  CtxFunctionArgScope TmpAS(FPS->getFName(), FPS->getIsStatic(), PSL);
  if (AllCtxFnArgScopes.find(TmpAS) == AllCtxFnArgScopes.end()) {
    AllCtxFnArgScopes.insert(TmpAS);
  }
  const auto &CFAS = *AllCtxFnArgScopes.find(TmpAS);
  return &CFAS;
}

const FunctionScope *FunctionScope::getFunctionScope(std::string FnName,
                                                     bool IsSt) {
  FunctionScope TmpFS(FnName, IsSt);
  if (AllFnScopes.find(TmpFS) == AllFnScopes.end()) {
    AllFnScopes.insert(TmpFS);
  }
  const auto &FS = *AllFnScopes.find(TmpFS);
  return &FS;
}

std::set<ProgramVar *> ProgramVar::AllProgramVars;

std::string ProgramVar::mkString(bool GetKey) {
  std::string Ret = "";
  if (GetKey) {
    Ret = std::to_string(K) + "_";
  }
  if (GetKey && IsConstant) {
    Ret += "Cons:";
  }
  Ret += VarName;
  return Ret;
}

ProgramVar *ProgramVar::makeCopy(BoundsKey NK) {
  ProgramVar *NewPVar =
      new ProgramVar(NK, this->VarName, this->VScope, this->IsConstant);
  return NewPVar;
}

std::string ProgramVar::verboseStr() {
  std::string Ret = mkString(true) + "(" + VScope->getStr() + ")";
  return Ret;
}

ProgramVar *ProgramVar::createNewProgramVar(BoundsKey VK, std::string VName,
                                            const ProgramVarScope *PVS,
                                            bool IsCons) {
  ProgramVar *NewPV = new ProgramVar(VK, VName, PVS, IsCons);
  AllProgramVars.insert(NewPV);
  return NewPV;
}