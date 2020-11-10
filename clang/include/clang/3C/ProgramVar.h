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

#include "clang/3C/PersistentSourceLoc.h"
#include "clang/AST/ASTContext.h"
#include <stdint.h>
#include <string>

// Unique ID for a program variable or constant literal, both of
// which could serve as bounds
typedef uint32_t BoundsKey;

// Class representing scope of a program variable.
class ProgramVarScope {
public:
  enum ScopeKind {
    // Function scope.
    FunctionScopeKind,
    // Parameters of a particular function.
    FunctionParamScopeKind,
    // All arguments to a particular call
    CtxFunctionArgScopeKind,
    // Struct scope.
    StructScopeKind,
    // Global scope.
    GlobalScopeKind,
  };
  ScopeKind getKind() const { return Kind; }

protected:
  ProgramVarScope(ScopeKind K) : Kind(K) {}
  ScopeKind Kind;

public:
  virtual ~ProgramVarScope() {}

  virtual bool operator==(const ProgramVarScope &) const = 0;
  virtual bool operator!=(const ProgramVarScope &) const = 0;
  virtual bool operator<(const ProgramVarScope &) const = 0;
  virtual std::string getStr() const = 0;
};

class PVSComp {
public:
  bool operator()(const ProgramVarScope &Lhs,
                  const ProgramVarScope &Rhs) const {
    return Lhs < Rhs;
  }
};

// Scope for all global variables and program constants.
class GlobalScope : public ProgramVarScope {
public:
  GlobalScope() : ProgramVarScope(GlobalScopeKind) {}
  virtual ~GlobalScope() {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == GlobalScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    return clang::isa<GlobalScope>(&O);
  }

  bool operator!=(const ProgramVarScope &O) const { return !(*this == O); }

  bool operator<(const ProgramVarScope &O) const { return false; }

  std::string getStr() const { return "Global"; }

  static GlobalScope *getGlobalScope();

private:
  static GlobalScope *ProgScope;
};

class StructScope : public ProgramVarScope {
public:
  StructScope(std::string SN) : ProgramVarScope(StructScopeKind), StName(SN) {}

  virtual ~StructScope() {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == StructScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    if (auto *SS = clang::dyn_cast<StructScope>(&O)) {
      return SS->StName == StName;
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const { return !(*this == O); }

  bool operator<(const ProgramVarScope &O) const {
    if (clang::isa<GlobalScope>(&O)) {
      return true;
    }

    if (auto *SS = clang::dyn_cast<StructScope>(&O)) {
      if (this->StName != SS->StName) {
        return StName < SS->StName;
      }
      return false;
    }

    return false;
  }

  std::string getStr() const { return "Struct_" + StName; }

  static const StructScope *getStructScope(std::string StName);

private:
  std::string StName;
  static std::set<StructScope, PVSComp> AllStScopes;
};

class FunctionParamScope : public ProgramVarScope {
public:
  friend class FunctionScope;
  FunctionParamScope(const std::string &FN, bool IsSt)
      : ProgramVarScope(FunctionParamScopeKind), FName(FN), IsStatic(IsSt) {}

  virtual ~FunctionParamScope() {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == FunctionParamScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    if (auto *FPS = clang::dyn_cast<FunctionParamScope>(&O)) {
      return (FPS->FName == FName && FPS->IsStatic == IsStatic);
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const { return !(*this == O); }

  bool operator<(const ProgramVarScope &O) const {
    if (clang::isa<GlobalScope>(&O) || clang::isa<StructScope>(&O)) {
      return true;
    }

    if (auto *FPS = clang::dyn_cast<FunctionParamScope>(&O)) {
      if (this->FName != FPS->FName) {
        return FName < FPS->FName;
      }
      if (this->IsStatic != FPS->IsStatic) {
        return IsStatic < FPS->IsStatic;
      }
      return false;
    }

    return false;
  }

  std::string getStr() const { return "FuncParm_" + FName; }

  const llvm::StringRef getFName() const { return this->FName; }

  bool getIsStatic() const { return IsStatic; }

  static const FunctionParamScope *getFunctionParamScope(std::string FnName,
                                                         bool IsSt);

protected:
  std::string FName;
  bool IsStatic;

private:
  static std::set<FunctionParamScope, PVSComp> AllFnParamScopes;
};

// Context-sensitive arguments scope.
class CtxFunctionArgScope : public FunctionParamScope {
public:
  friend class FunctionScope;
  CtxFunctionArgScope(const std::string &FN, bool IsSt,
                      const PersistentSourceLoc &CtxPSL)
      : FunctionParamScope(FN, IsSt) {
    PSL = CtxPSL;
    std::string FileName = PSL.getFileName();
    CtxIDStr = "";
    if (!FileName.empty()) {
      llvm::sys::fs::UniqueID UId;
      if (llvm::sys::fs::getUniqueID(FileName, UId)) {
        CtxIDStr = std::to_string(UId.getDevice()) + ":" +
                   std::to_string(UId.getFile()) + ":";
      }
    }
    CtxIDStr +=
        std::to_string(PSL.getLineNo()) + ":" + std::to_string(PSL.getColSNo());
    this->Kind = CtxFunctionArgScopeKind;
  }

  virtual ~CtxFunctionArgScope() {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == CtxFunctionArgScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    if (auto *FPS = clang::dyn_cast<CtxFunctionArgScope>(&O)) {
      return (FPS->FName == FName && FPS->IsStatic == IsStatic &&
              !(FPS->PSL < PSL || PSL < FPS->PSL));
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const { return !(*this == O); }

  bool operator<(const ProgramVarScope &O) const {
    if (clang::isa<GlobalScope>(&O) || clang::isa<FunctionParamScope>(&O) ||
        clang::isa<StructScope>(&O)) {
      return true;
    }

    if (auto *FPS = clang::dyn_cast<CtxFunctionArgScope>(&O)) {
      if (this->FName != FPS->FName) {
        return FName < FPS->FName;
      }
      if (this->IsStatic != FPS->IsStatic) {
        return IsStatic < FPS->IsStatic;
      }
      if (FPS->PSL < PSL || PSL < FPS->PSL) {
        return PSL < FPS->PSL;
      }
      return false;
    }

    return false;
  }

  std::string getStr() const { return FName + "_Ctx_" + CtxIDStr; }

  static const CtxFunctionArgScope *
  getCtxFunctionParamScope(const FunctionParamScope *FPS,
                           const PersistentSourceLoc &PSL);

private:
  PersistentSourceLoc PSL; // source code location of this function call
  std::string CtxIDStr;
  static std::set<CtxFunctionArgScope, PVSComp> AllCtxFnArgScopes;
};

class FunctionScope : public ProgramVarScope {
public:
  FunctionScope(std::string FN, bool IsSt)
      : ProgramVarScope(FunctionScopeKind), FName(FN), IsStatic(IsSt) {}

  virtual ~FunctionScope() {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == FunctionScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const {
    if (auto *FS = clang::dyn_cast<FunctionScope>(&O)) {
      return (FS->FName == FName && FS->IsStatic == IsStatic);
    }
    if (auto *FPS = clang::dyn_cast<FunctionParamScope>(&O)) {
      return (FPS->FName == FName && FPS->IsStatic == IsStatic);
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const { return !(*this == O); }

  bool operator<(const ProgramVarScope &O) const {
    if (clang::isa<GlobalScope>(&O) || clang::isa<FunctionParamScope>(&O) ||
        clang::isa<CtxFunctionArgScope>(&O) || clang::isa<StructScope>(&O)) {
      return true;
    }

    if (auto *FS = clang::dyn_cast<FunctionScope>(&O)) {
      if (this->FName != FS->FName) {
        return FName < FS->FName;
      }
      if (this->IsStatic != FS->IsStatic) {
        return IsStatic < FS->IsStatic;
      }
      return false;
    }
    return false;
  }

  std::string getStr() const { return "InFunc_" + FName; }

  static const FunctionScope *getFunctionScope(std::string FnName, bool IsSt);

private:
  std::string FName;
  bool IsStatic;

  static std::set<FunctionScope, PVSComp> AllFnScopes;
};

// Class that represents a program variable along with its scope.
class ProgramVar {
public:
  const ProgramVarScope *getScope() { return VScope; }
  void setScope(const ProgramVarScope *PVS) { this->VScope = PVS; }
  BoundsKey getKey() { return K; }
  bool IsNumConstant() { return IsConstant; }
  std::string mkString(bool GetKey = false);
  std::string getVarName() { return VarName; }
  std::string verboseStr();
  ProgramVar *makeCopy(BoundsKey NK);
  virtual ~ProgramVar() {}

  static ProgramVar *createNewProgramVar(BoundsKey VK, std::string VName,
                                         const ProgramVarScope *PVS,
                                         bool IsCons = false);

private:
  BoundsKey K;
  std::string VarName;
  const ProgramVarScope *VScope;
  bool IsConstant; // is a literal integer, not a variable
  // TODO: All the ProgramVars may not be used. We should try to figure out
  //  a way to free unused program vars.
  static std::set<ProgramVar *> AllProgramVars;

  ProgramVar(BoundsKey VK, std::string VName, const ProgramVarScope *PVS,
             bool IsCons)
      : K(VK), VarName(VName), VScope(PVS), IsConstant(IsCons) {}

  ProgramVar(BoundsKey VK, std::string VName, const ProgramVarScope *PVS)
      : ProgramVar(VK, VName, PVS, false) {}
};

#endif // _BOUNDSVAR_H
