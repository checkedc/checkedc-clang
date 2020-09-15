//=--ProgramVar.h-------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains ProgramVar and related classes that are used to represent
// program variables.
//
//===----------------------------------------------------------------------===//

#ifndef _BOUNDSVAR_H
#define _BOUNDSVAR_H

#include <stdint.h>
#include <string>
#include "clang/AST/ASTContext.h"
#include "clang/CConv/PersistentSourceLoc.h"

typedef uint32_t BoundsKey;

// Class representing scope of a program variable.
class ProgramVarScope {
public:
  enum ScopeKind {
    // Function scope.
    FunctionScopeKind,
    // Function parameter scope.
    FunctionParamScopeKind,
    // Context sensitive argument scope.
    CtxFunctionArgScopeKind,
    // Struct scope.
    StructScopeKind,
    // Global scope.
    GlobalScopeKind,
  };
  ScopeKind getKind() const { return Kind; }

protected:
  ProgramVarScope(ScopeKind K): Kind(K) { }
  ScopeKind Kind;
public:
  virtual ~ProgramVarScope() { }

  virtual bool operator==(const ProgramVarScope &) const = 0;
  virtual bool operator!=(const ProgramVarScope &) const = 0;
  virtual bool operator<(const ProgramVarScope &) const = 0;
  virtual std::string getStr() const = 0;

};

// Scope for all global variables and program constants.
class GlobalScope : public ProgramVarScope {
public:
  GlobalScope() :
  ProgramVarScope(GlobalScopeKind) { }
  virtual ~GlobalScope() { }

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == GlobalScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    return clang::isa<GlobalScope>(&O);
  }

  bool operator!=(const ProgramVarScope &O) const {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const {
    return false;
  }

  std::string getStr() const {
    return "Global";
  }

  static GlobalScope *getGlobalScope();
private:
  static GlobalScope *ProgScope;
};

class StructScope : public ProgramVarScope {
public:
  StructScope(std::string SN) :
  ProgramVarScope(StructScopeKind),
  StName(SN) { }

  virtual ~StructScope() { }

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == StructScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    if (const StructScope *SS = clang::dyn_cast<StructScope>(&O)) {
      return SS->StName == StName;
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const {
    return clang::isa<GlobalScope>(&O);
  }

  std::string getStr() const {
    return "Struct_" + StName;
  }

  static StructScope *getStructScope(std::string StName);

private:
  std::string StName;
  static std::map<std::string, StructScope *> StScopeMap;
};

class FunctionScope;

class FunctionParamScope : public ProgramVarScope {
public:
  friend class FunctionScope;
  FunctionParamScope(std::string FN, bool IsSt) :
      ProgramVarScope(FunctionParamScopeKind),
      FName(FN), IsStatic(IsSt) { }

  virtual ~FunctionParamScope() { }

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == FunctionParamScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    if (const FunctionParamScope *FPS =
        clang::dyn_cast<FunctionParamScope>(&O)) {
      return (FPS->FName == FName && FPS->IsStatic == IsStatic);
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const {
    return clang::isa<GlobalScope>(&O);
  }

  std::string getStr() const {
    return "FuncParm_" + FName;
  }

  std::string getFName() const {
    return FName;
  }

  bool getIsStatic() const {
    return IsStatic;
  }

  static FunctionParamScope *
        getFunctionParamScope(std::string FnName, bool IsSt);

protected:
  std::string FName;
  bool IsStatic;
private:
  static std::map<std::pair<std::string, bool>,
                  FunctionParamScope *> FnParmScopeMap;
};

// Context-sensitive arguments scope.
class CtxFunctionArgScope : public FunctionParamScope {
public:
  friend class FunctionScope;
  CtxFunctionArgScope(std::string FN, bool IsSt,
                        const PersistentSourceLoc &CtxPSL) :
    FunctionParamScope(FN, IsSt) {
    PSL = CtxPSL;
    this->Kind = CtxFunctionArgScopeKind;
  }

  virtual ~CtxFunctionArgScope() { }

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == CtxFunctionArgScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    if (const CtxFunctionArgScope *FPS =
      clang::dyn_cast<CtxFunctionArgScope>(&O)) {
      return (FPS->FName == FName &&
              FPS->IsStatic == IsStatic &&
              !(FPS->PSL < PSL || PSL < FPS->PSL));
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const {
    return clang::isa<GlobalScope>(&O);
  }

  std::string getStr() const {
    return "CtxFuncArg_" + FName;
  }

  static CtxFunctionArgScope *
  getCtxFunctionParamScope(FunctionParamScope *FPS,
                           const PersistentSourceLoc &PSL);

private:
  PersistentSourceLoc PSL;

  static std::map<std::tuple<std::string, bool, PersistentSourceLoc>,
                  CtxFunctionArgScope *> CtxFnArgScopeMap;
};

class FunctionScope : public ProgramVarScope {
public:
  FunctionScope(std::string FN, bool IsSt) :
                ProgramVarScope(FunctionScopeKind),
                FName(FN), IsStatic(IsSt) { }

  virtual ~FunctionScope() { }

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == FunctionScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    if (const FunctionScope *FS =
      clang::dyn_cast<FunctionScope>(&O)) {
      return (FS->FName == FName && FS->IsStatic == IsStatic);
    }
    if (const FunctionParamScope *FPS =
      clang::dyn_cast<FunctionParamScope>(&O)) {
      return (FPS->FName == FName && FPS->IsStatic == IsStatic);
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const {
    return clang::isa<GlobalScope>(&O);
  }

  std::string getStr() const {
    return "InFunc_" + FName;
  }

  static FunctionScope *getFunctionScope(std::string FnName,
                                         bool IsSt);

private:
  std::string FName;
  bool IsStatic;

  static std::map<std::pair<std::string, bool>, FunctionScope *>
    FnScopeMap;
};

// Class that represents a program variable along with its scope.
class ProgramVar {
public:
  ProgramVar(BoundsKey VK, std::string VName, ProgramVarScope *PVS,
             bool IsCons) :
      K(VK), VarName(VName), VScope(PVS), IsConstant(IsCons) { }

  ProgramVar(BoundsKey VK, std::string VName, ProgramVarScope *PVS) :
      ProgramVar(VK, VName, PVS, false) { }

  ProgramVarScope *getScope() { return VScope; }
  void setScope(ProgramVarScope *PVS) { this->VScope = PVS; }
  BoundsKey getKey() { return K; }
  bool IsNumConstant() { return IsConstant; }
  std::string mkString(bool GetKey = false);
  std::string getVarName() { return VarName; }
  std::string verboseStr();
  ProgramVar *makeCopy(BoundsKey NK);
  virtual ~ProgramVar() { }
private:
  BoundsKey K;
  std::string VarName;
  ProgramVarScope *VScope;
  bool IsConstant;
};

#endif // _BOUNDSVAR_H
