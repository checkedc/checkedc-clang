//=--IterativeItypeHelper.cpp-------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of IterativeItypeHelper methods.
//===----------------------------------------------------------------------===//

#include "clang/CConv/IterativeItypeHelper.h"

// Map that stored the newly detected itype parameters and
// returns that are detected in this iteration.
static Constraints::EnvironmentMap IterationItypeMap;
// Map that contains the constraint atoms of parameters
// and its return for all functions (including their declarations
// and definitions). This map is used to determine new detection of itypes.
static std::map<std::string,
                std::map<Atom*, ConstAtom*>>
    ParamsReturnSavedValues;

// This method saves the constraint vars of parameters and return of all
// the provided FVConstraint vars with default value as null.
// These are used to later check if anything has changed, in which case the
// corresponding function will be considered as modified.
static void
updateFunctionConstraintVars(std::string FuncKey,
                             Constraints &CS,
                             std::set<ConstraintVariable *> &FVConstVars) {
  for (auto TopVar : FVConstVars) {
    // If this is a function constraint?
    if (FVConstraint *FvCons = dyn_cast<FVConstraint>(TopVar)) {
      // Update the variables of function parameters.
      for (unsigned i = 0; i < FvCons->numParams(); i++) {
        for (ConstraintVariable *PVar : FvCons->getParamVar(i)) {
          assert(dyn_cast<PVConstraint>(PVar) && "Expected a pointer "
                                                     "variable constraint.");
          PVConstraint *PvConst = dyn_cast<PVConstraint>(PVar);
          for (auto A : PvConst->getCvars()) {
            ParamsReturnSavedValues[FuncKey][A] = nullptr;
          }
        }
      }
      // Update the variables of function return vars.
      for (ConstraintVariable *ReturnVar : FvCons->getReturnVars()) {
        assert(dyn_cast<PVConstraint>(ReturnVar) && "Expected a pointer "
                                                    "variable constraint.");
        PVConstraint *RetVarCons = dyn_cast<PVConstraint>(ReturnVar);
        for (auto A : RetVarCons->getCvars()) {
          ParamsReturnSavedValues[FuncKey][A] = nullptr;
        }
      }
    }
  }
}

bool identifyModifiedFunctions(Constraints &CS,
                               std::set<std::string> &ModFuncs) {
  ModFuncs.clear();
  // Get the current values.
  Constraints::EnvironmentMap &EnvMap = CS.getVariables();
  // Check to see if they differ from previous values.
  for (auto &FuncVals : ParamsReturnSavedValues) {
    std::string DefKey = FuncVals.first;
    for (auto &CurrVar : FuncVals.second) {
      // Check if the value of the constraint variable changed?
      // then we consider the corresponding function as modified.
      if (VarAtom *VA = dyn_cast<VarAtom>(CurrVar.first)) {
        if (EnvMap[VA] != CurrVar.second) {
          CurrVar.second = EnvMap[VA];
          ModFuncs.insert(DefKey);
        }
      }
    }
  }
  return !ModFuncs.empty();
}

unsigned long resetWithitypeConstraints(Constraints &CS) {
  Constraints::EnvironmentMap DeclConstraints;
  DeclConstraints.clear();
  Constraints::EnvironmentMap &EnvMap = CS.getVariables();
  unsigned long Removed = 0;

  Constraints::EnvironmentMap ToRemoveVAtoms;

  // Restore the erased constraints.
  // Now, try to remove constraints that
  // depend on ityped constraint variables.

  // Make a map of constraints to remove.
  for (auto &ITypeVar : IterationItypeMap) {
    ConstAtom *targetCons = ITypeVar.second;
    if (!dyn_cast<NTArrAtom>(ITypeVar.second)) {
      targetCons = nullptr;
    }
    ToRemoveVAtoms[ITypeVar.first] = targetCons;
  }

  // Now try to remove the constraints.
  for (auto &CurrE : EnvMap) {
    CurrE.first->resetErasedConstraints();
    Removed += CurrE.first->replaceEqConstraints(ToRemoveVAtoms, CS);
  }

  // Check if we removed any constraints?
  if (Removed > 0) {
    // We removed constraints.
    // Reset everything.

    // Backup the computed results of
    // declaration parameters and returns.
    for (auto &ITypeVar : CS.getitypeVarMap()) {
      DeclConstraints[ITypeVar.first] =
          EnvMap[CS.getVar(ITypeVar.first->getLoc())];
    }

    // Reset all constraints to Ptrs.
    CS.resetConstraints();

    // Restore the precomputed constraints for declarations.
    for (auto &ITypeVar : DeclConstraints) {
      EnvMap[CS.getVar(ITypeVar.first->getLoc())] = ITypeVar.second;
    }
  }

  return Removed;

}

// This method updates the pointer type of the declaration constraint variable
// with the type of the definition constraint variable.
static bool updateDeclWithDefnType(ConstraintVariable *Pdecl,
                                   ConstraintVariable *Pdefn,
                                   ProgramInfo &Info) {
  Constraints &CS = Info.getConstraints();
  bool Changed = false;
  // Get the itype map where we store the pointer type of
  // the declaration constraint variables.
  Constraints::EnvironmentMap &ItypeMap = CS.getitypeVarMap();
  PVConstraint *PVDeclCons = dyn_cast<PVConstraint>(Pdecl);
  PVConstraint *PVDefnCons = dyn_cast<PVConstraint>(Pdefn);

  // These has to be pointer constraint variables.
  assert(PVDeclCons != nullptr && PVDefnCons != nullptr &&
         "Expected a pointer variable constraint for function parameter "
         "but got nullptr");

  // Get the pointer type of the top level definition constraint variable.
  ConstAtom *ItypeAtom = CS.getAssignment(*(PVDefnCons->getCvars().begin()));
  auto DeclTopCVar = *(PVDeclCons->getCvars().begin());

  assert(ItypeAtom != nullptr && "Unable to find assignment for definition "
                                 "constraint variable.");

  if (VarAtom *VA = dyn_cast<VarAtom>(DeclTopCVar)) {
    if (ItypeMap.find(VA) == ItypeMap.end() || ItypeMap[VA] != ItypeAtom) {
      // Update the type of the declaration constraint variable.
      ItypeMap[VA] = ItypeAtom;
      IterationItypeMap[VA] = ItypeAtom;
      Changed = true;
    }
  }

  return Changed;
}

unsigned long detectAndUpdateITypeVars(ProgramInfo &Info,
                                       std::set<std::string> &ModFuncs) {
  Constraints &CS = Info.getConstraints();
  unsigned long NumITypeVars = 0;
  // Clear the current iteration itype vars.
  IterationItypeMap.clear();
  for (auto FuncDefKey : ModFuncs) {
    FVConstraint *CDefn =
        getHighestT<FVConstraint>(CS.getFuncDefnVarMap()[FuncDefKey], Info);

    auto DeclConstraintsPtr = Info.getFuncDeclConstraintSet(FuncDefKey);
    assert(DeclConstraintsPtr != nullptr && "This cannot be nullptr, "
                                            "if it was null, we would never "
                                            "have inserted this info into "
                                            "modified functions.");
    FVConstraint *CDecl = getHighestT<FVConstraint>(*DeclConstraintsPtr, Info);

    assert(CDecl != nullptr);
    assert(CDefn != nullptr);

    if (CDecl->numParams() == CDefn->numParams()) {
      // Compare parameters.
      for (unsigned i = 0; i < CDecl->numParams(); ++i) {
        auto Decl = getHighestT<PVConstraint>(CDecl->getParamVar(i), Info);
        auto Defn = getHighestT<PVConstraint>(CDefn->getParamVar(i), Info);
        if (ProgramInfo::isAValidPVConstraint(Decl) &&
            ProgramInfo::isAValidPVConstraint(Defn)) {
          auto HeadDeclCVar = *(Decl->getCvars().begin());
          auto HeadDefnCVar = *(Defn->getCvars().begin());

          // Definition is more precise than declaration
          // and declaration has to be WILD.
          // If this holds, then we want to insert a bounds safe interface.
          if (!CS.isWild(HeadDefnCVar) && CS.isWild(HeadDeclCVar) &&
              updateDeclWithDefnType(Decl, Defn, Info)) {
            NumITypeVars++;
          }
        }
      }
    }

    // Compare returns.
    auto Decl = getHighestT<PVConstraint>(CDecl->getReturnVars(), Info);
    auto Defn = getHighestT<PVConstraint>(CDefn->getReturnVars(), Info);

    if (ProgramInfo::isAValidPVConstraint(Decl) &&
        ProgramInfo::isAValidPVConstraint(Defn)) {

      auto HeadDeclCVar = *(Decl->getCvars().begin());
      auto HeadDefnCVar = *(Defn->getCvars().begin());

      // Definition is more precise than declaration
      // and declaration has to be WILD.
      if (!CS.isWild(HeadDefnCVar) && CS.isWild(HeadDeclCVar) &&
          updateDeclWithDefnType(Decl, Defn, Info)) {
        NumITypeVars++;
      }
    }
  }
  return NumITypeVars;
}

bool performConstraintSetup(ProgramInfo &Info) {
  bool Ret = false;
  Constraints &CS = Info.getConstraints();
  for (auto &FDef : CS.getFuncDefnVarMap()) {
    // Get the key for the function definition.
    auto DefKey = FDef.first;
    std::set<ConstraintVariable *> &DefCVars = FDef.second;

    std::set<ConstraintVariable *> *DeclCVarsPtr =
        Info.getFuncDeclConstraintSet(DefKey);

    if (DeclCVarsPtr != nullptr) {
      // Okay, we have constraint variables for declaration.
      // There could be a possibility of itypes.
      // Save the var atoms.
      updateFunctionConstraintVars(DefKey, CS, DefCVars);
      Ret = true;
    }
  }
  return Ret;
}

