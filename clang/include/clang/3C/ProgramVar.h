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

#ifndef LLVM_CLANG_3C_PROGRAMVAR_H
#define LLVM_CLANG_3C_PROGRAMVAR_H

#include "clang/3C/PersistentSourceLoc.h"
#include "clang/AST/ASTContext.h"
#include <stdint.h>
#include <string>

// Unique ID for a program variable or constant literal, both of
// which could serve as bounds
typedef uint32_t BoundsKey;

typedef std::set<BoundsKey> BKeySet;

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
    // Context sensitive struct scope
    CtxStructScopeKind,
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
  // Variables of scope x are all visible in y iff y.isInInnerScope(x)
  // is true.
  virtual bool isInInnerScope(const ProgramVarScope &) const = 0;
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
  ~GlobalScope() override {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == GlobalScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const override {
    return clang::isa<GlobalScope>(&O);
  }

  bool operator!=(const ProgramVarScope &O) const override {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const override { return false; }

  bool isInInnerScope(const ProgramVarScope &O) const override { return false; }

  std::string getStr() const override { return "Global"; }

  static GlobalScope *getGlobalScope();

private:
  static GlobalScope *ProgScope;
};

class StructScope : public ProgramVarScope {
public:
  StructScope(std::string SN) : ProgramVarScope(StructScopeKind), StName(SN) {}

  ~StructScope() override {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == StructScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const override {
    if (auto *SS = clang::dyn_cast<StructScope>(&O)) {
      return SS->StName == StName;
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const override {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const override {
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

  bool isInInnerScope(const ProgramVarScope &O) const override {
    // only global variables are visible here.
    return clang::isa<GlobalScope>(&O);
  }

  std::string getStr() const override { return "Struct_" + StName; }

  std::string getSName() const { return this->StName; }

  static const StructScope *getStructScope(std::string StName);

protected:
  std::string StName;

private:
  static std::set<StructScope, PVSComp> AllStScopes;
};

class CtxStructScope : public StructScope {
public:
  CtxStructScope(const std::string &SN, const std::string &AS, bool &IsGlobal)
      : StructScope(SN), ASKey(AS), IsG(IsGlobal) {
    this->Kind = CtxStructScopeKind;
  }

  ~CtxStructScope() override {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == CtxStructScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const override {
    if (auto *CS = clang::dyn_cast<CtxStructScope>(&O)) {
      return CS->StName == StName && CS->ASKey == ASKey && CS->IsG == IsG;
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const override {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const override {
    if (clang::isa<GlobalScope>(&O) || clang::isa<StructScope>(&O)) {
      return true;
    }

    if (auto *CS = clang::dyn_cast<CtxStructScope>(&O)) {
      if (this->ASKey != CS->ASKey) {
        return ASKey < CS->ASKey;
      }
      if (this->StName != CS->StName) {
        return StName < CS->StName;
      }
      if (this->IsG != CS->IsG) {
        return IsG < CS->IsG;
      }
      return false;
    }

    return false;
  }

  std::string getStr() const override {
    return "CtxStruct_" + ASKey + "_" + StName + "_" + std::to_string(IsG);
  }

  static const CtxStructScope *getCtxStructScope(const StructScope *SS,
                                                 std::string AS, bool IsGlobal);

private:
  std::string ASKey;
  bool IsG;
  static std::set<CtxStructScope, PVSComp> AllCtxStScopes;
};

class FunctionParamScope : public ProgramVarScope {
public:
  friend class FunctionScope;
  FunctionParamScope(const std::string &FN, bool IsSt)
      : ProgramVarScope(FunctionParamScopeKind), FName(FN), IsStatic(IsSt) {}

  ~FunctionParamScope() override {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == FunctionParamScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const override {
    if (auto *FPS = clang::dyn_cast<FunctionParamScope>(&O)) {
      return (FPS->FName == FName && FPS->IsStatic == IsStatic);
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const override {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const override {
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

  bool isInInnerScope(const ProgramVarScope &O) const override {
    // only global variables are visible here.
    return clang::isa<GlobalScope>(&O);
  }

  std::string getStr() const override { return "FuncParm_" + FName; }

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

  ~CtxFunctionArgScope() override {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == CtxFunctionArgScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const override {
    if (auto *FPS = clang::dyn_cast<CtxFunctionArgScope>(&O)) {
      return (FPS->FName == FName && FPS->IsStatic == IsStatic &&
              !(FPS->PSL < PSL || PSL < FPS->PSL));
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const override {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const override {
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

  std::string getStr() const override { return FName + "_Ctx_" + CtxIDStr; }

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

  ~FunctionScope() override {}

  static bool classof(const ProgramVarScope *S) {
    return S->getKind() == FunctionScopeKind;
  }

  bool operator==(const ProgramVarScope &O) const override {
    if (auto *FS = clang::dyn_cast<FunctionScope>(&O)) {
      return (FS->FName == FName && FS->IsStatic == IsStatic);
    }
    if (auto *FPS = clang::dyn_cast<FunctionParamScope>(&O)) {
      return (FPS->FName == FName && FPS->IsStatic == IsStatic);
    }
    return false;
  }

  bool operator!=(const ProgramVarScope &O) const override {
    return !(*this == O);
  }

  bool operator<(const ProgramVarScope &O) const override {
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

  bool isInInnerScope(const ProgramVarScope &O) const override;

  std::string getStr() const override { return "InFunc_" + FName; }

  static const FunctionScope *getFunctionScope(std::string FnName, bool IsSt);

private:
  std::string FName;
  bool IsStatic;

  static std::set<FunctionScope, PVSComp> AllFnScopes;
};

// Class that represents a program variable along with its scope.
class ProgramVar {
public:
  const ProgramVarScope *getScope() const { return VScope; }
  void setScope(const ProgramVarScope *PVS) { this->VScope = PVS; }
  BoundsKey getKey() const { return K; }
  const std::string &getVarName() const { return VarName; }
  std::string verboseStr() const;
  ProgramVar *makeCopy(BoundsKey NK) const;

  bool isNumConstant() const {return IsConstant; }
  uint64_t getConstantVal() const {
    assert("Can't get constant value for non-constant var." && IsConstant);
    return ConstantVal;
  }

  static ProgramVar *createNewProgramVar(BoundsKey VK, std::string VName,
                                         const ProgramVarScope *PVS);

  static ProgramVar *createNewConstantVar(BoundsKey VK, uint64_t Value);

private:
  BoundsKey K;
  std::string VarName;
  const ProgramVarScope *VScope;

  // Is a literal integer, not a variable.
  bool IsConstant;
  uint64_t ConstantVal;

  // TODO: All the ProgramVars may not be used. We should try to figure out
  //  a way to free unused program vars.
  static std::set<const ProgramVar *> AllProgramVars;

  ProgramVar(BoundsKey K, const std::string &VarName,
             const ProgramVarScope *VScope, bool IsConstant,
             uint32_t ConstantVal)
    : K(K), VarName(VarName), VScope(VScope), IsConstant(IsConstant),
      ConstantVal(ConstantVal) {
    // Constant variables should be a subclass of ProgramVariable. Until that
    // change happens this should sanity check how ProgramVars are constructed.
    assert("Constant value should not be set for non-constant variables." &&
           (IsConstant || ConstantVal == 0));
    AllProgramVars.insert(this);
  }

  ProgramVar(BoundsKey VK, std::string VName, const ProgramVarScope *PVS)
    : ProgramVar(VK, VName, PVS, false, 0) {}

  ProgramVar(BoundsKey VK, uint32_t CVal)
    : ProgramVar(VK, std::to_string(CVal), GlobalScope::getGlobalScope(), true,
                 CVal) {}
};

#endif // LLVM_CLANG_3C_PROGRAMVAR_H
