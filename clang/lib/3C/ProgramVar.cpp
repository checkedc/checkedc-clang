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
std::set<CtxStructScope, PVSComp> CtxStructScope::AllCtxStScopes;
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

const CtxStructScope *CtxStructScope::getCtxStructScope(const StructScope *SS,
                                                        std::string AS,
                                                        bool IsGlobal) {
  CtxStructScope TmpCSS(SS->getSName(), AS, IsGlobal);
  if (AllCtxStScopes.find(TmpCSS) == AllCtxStScopes.end()) {
    AllCtxStScopes.insert(TmpCSS);
  }
  const auto &CSS = *AllCtxStScopes.find(TmpCSS);
  return &CSS;
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

bool FunctionScope::isInInnerScope(const ProgramVarScope &O) const {
  // Global variables and function parameters are visible here.
  if (clang::isa<GlobalScope>(&O))
    return true;

  // Function parameters of the same function are also visible
  // inside the function.
  if (auto *FPS = clang::dyn_cast<FunctionParamScope>(&O)) {
    if (this->FName == FPS->getFName() &&
        this->IsStatic == FPS->getIsStatic()) {
      return true;
    }
  }
  return false;
}

const CtxFunctionArgScope *
CtxFunctionArgScope::getCtxFunctionParamScope(const FunctionParamScope *FPS,
                                              const PersistentSourceLoc &PSL) {
  CtxFunctionArgScope TmpAS(std::string(FPS->getFName()), FPS->getIsStatic(),
                            PSL);
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

std::set<const ProgramVar *> ProgramVar::AllProgramVars;

std::string ProgramVar::verboseStr() const {
  std::string Ret = std::to_string(K) + "_";
  if (IsConstant)
    Ret += "Cons:";
  return Ret + VarName + "(" + VScope->getStr() + ")";
}

ProgramVar *ProgramVar::makeCopy(BoundsKey NK) const {
  return new ProgramVar(NK, this->VarName, this->VScope, this->IsConstant,
                        this->ConstantVal);
}

ProgramVar *ProgramVar::createNewConstantVar(BoundsKey VK,
                                             uint64_t Value) {
  return new ProgramVar(VK, Value);
}

ProgramVar *ProgramVar::createNewProgramVar(BoundsKey VK, std::string VName,
                                            const ProgramVarScope *PVS) {
  return new ProgramVar(VK, VName, PVS);
}