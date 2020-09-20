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

#include "clang/CConv/ProgramVar.h"

GlobalScope *GlobalScope::ProgScope = nullptr;
std::map<std::string, StructScope*> StructScope::StScopeMap;
std::map<std::pair<std::string, bool>, FunctionScope*>
    FunctionScope::FnScopeMap;
std::map<std::pair<std::string, bool>, FunctionParamScope*>
    FunctionParamScope::FnParmScopeMap;

GlobalScope *GlobalScope::getGlobalScope() {
  if (ProgScope == nullptr) {
    ProgScope = new GlobalScope();
  }
  return ProgScope;
}

StructScope *StructScope::getStructScope(std::string StName) {
  if (StScopeMap.find(StName) == StScopeMap.end()) {
    StScopeMap[StName] = new StructScope(StName);
  }
  return StScopeMap[StName];
}

FunctionParamScope *FunctionParamScope::getFunctionParamScope(
    std::string FnName, bool IsSt) {
  auto MapK = std::make_pair(FnName, IsSt);
  if (FnParmScopeMap.find(MapK) == FnParmScopeMap.end()) {
    FnParmScopeMap[MapK] = new FunctionParamScope(FnName, IsSt);
  }
  return FnParmScopeMap[MapK];
}

FunctionScope *FunctionScope::getFunctionScope(std::string FnName, bool IsSt) {
  auto MapK = std::make_pair(FnName, IsSt);
  if (FnScopeMap.find(MapK) == FnScopeMap.end()) {
    FnScopeMap[MapK] = new FunctionScope(FnName, IsSt);
  }
  return FnScopeMap[MapK];
}


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
  ProgramVar *NewPVar = new ProgramVar(NK, this->VarName, this->VScope,
                                       this->IsConstant);
  return NewPVar;
}

std::string ProgramVar::verboseStr() {
  std::string Ret = mkString(true) + "(" + VScope->getStr() + ")";
  return Ret;
}