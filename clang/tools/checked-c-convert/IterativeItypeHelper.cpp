//=--IterativeItypeHelper.cpp-------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of IterativeItypeHelper methods.
//===----------------------------------------------------------------------===//

#include "IterativeItypeHelper.h"

// Map that stored the newly detected itype parameters and
// returns that are detected in this iteration.
static Constraints::EnvironmentMap IterationItypeMap;
// Map that contains the constraint variables of parameters
// and its return for all functions (including their declarations
// and definitions).
// This map is used to determine new detection of itypes.
static std::map<std::string, std::map<VarAtom *, ConstAtom *>>
    ParamsReturnSavedValues;

// This method saves the constraint vars of parameters and return of all the
// provided FVConstraint vars with default value as null.
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
          for (auto ConsVar : PvConst->getCvars()) {
            VarAtom *CurrVarAtom = CS.getVar(ConsVar);
            // Default value is null.
            ParamsReturnSavedValues[FuncKey][CurrVarAtom] = nullptr;
          }
        }
      }
      // Now, update the variables of function return vars.
      for (ConstraintVariable *ReturnVar : FvCons->getReturnVars()) {
        assert(dyn_cast<PVConstraint>(ReturnVar) && "Expected a pointer "
                                                    "variable constraint.");
        PVConstraint *RetVarCons = dyn_cast<PVConstraint>(ReturnVar);
        for (auto ConsVar : RetVarCons->getCvars()) {
          VarAtom *CurrVarAtom = CS.getVar(ConsVar);
          // The default value is also null here.
          ParamsReturnSavedValues[FuncKey][CurrVarAtom] = nullptr;
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
      // Then we consider the corresponding function as modified.
      if (EnvMap[CurrVar.first] != CurrVar.second) {
        CurrVar.second = EnvMap[CurrVar.first];
        ModFuncs.insert(DefKey);
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

  // Restore the erased constraints and try to remove constraints that
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
    // We removed constraints; reset everything.

    // Backup the computed results of declaration parameters and returns.
    for (auto &CurrITypeVar : CS.getitypeVarMap()) {
      DeclConstraints[CurrITypeVar.first] =
          EnvMap[CS.getVar(CurrITypeVar.first->getLoc())];
    }

    // Reset all constraints to Ptrs.
    CS.resetConstraints();

    // Restore the precomputed constraints for declarations.
    for (auto &CurrITypeVar : DeclConstraints) {
      EnvMap[CS.getVar(CurrITypeVar.first->getLoc())] = CurrITypeVar.second;
    }
  }

  return Removed;

}

// This method updates the pointer type of the declaration constraint variable
// with the type of the definition constraint variable.
static bool updateDeclWithDefnType(ConstraintVariable *PDecl,
                                   ConstraintVariable *PDefn,
                                   ProgramInfo &Info) {
  Constraints &CS = Info.getConstraints();
  bool Changed = false;
  // Get the itype map where we store the pointer type of the
  // declaration constraint variables.
  Constraints::EnvironmentMap &ItypeMap = CS.getitypeVarMap();
  PVConstraint *PVDeclCons = dyn_cast<PVConstraint>(PDecl);
  PVConstraint *PVDefnCons = dyn_cast<PVConstraint>(PDefn);

  // These has to be pointer constraint variables.
  assert(PVDeclCons != nullptr && PVDefnCons != nullptr &&
         "Expected a pointer variable constraint for function "
         "parameter but got nullptr");

  ConstAtom *ItypeAtom = nullptr;

  // Get the pointer type of the definition constraint variable.
  for (ConstraintKey k: PVDefnCons->getCvars()) {
    ItypeAtom = CS.getVariables()[CS.getVar(k)];
  }
  assert(ItypeAtom != nullptr && "Unable to find assignment for "
                                 "definition constraint variable.");

  // Check if this is already identified itype.
  for (ConstraintKey K : PVDeclCons->getCvars()) {
    VarAtom *CK = CS.getVar(K);
    if (ItypeMap.find(CK) != ItypeMap.end() && ItypeMap[CK] == ItypeAtom) {
        //Yes, then no need to do anything.
        return Changed;
    }
  }

  // Update the type of the declaration constraint variable.
  for (ConstraintKey k: PVDeclCons->getCvars()) {
    VarAtom *CK = CS.getVar(k);
    ItypeMap[CK] = ItypeAtom;
    IterationItypeMap[CK] = ItypeAtom;
    Changed = true;
  }
  return Changed;
}

unsigned long
detectAndUpdateITypeVars(ProgramInfo &Info,
                         std::set<std::string> &ModFuncs) {
  Constraints &CS = Info.getConstraints();
  unsigned long NumITypeVars = 0;
  // Clear the current iteration itype vars.
  IterationItypeMap.clear();
  for (auto funcDefKey: ModFuncs) {
    FVConstraint *CDefn =
        dyn_cast<FVConstraint>(getHighest(CS.getFuncDefnVarMap()[funcDefKey],
                                          Info));

    auto DeclConstraintsPtr = Info.getFuncDeclConstraintSet(funcDefKey);
    assert(DeclConstraintsPtr != nullptr && "This cannot be nullptr, if it was "
                                            "null, we would never have "
                                            "inserted this info modified "
                                            "functions.");
    FVConstraint *CDecl =
        dyn_cast<FVConstraint>(getHighest(*DeclConstraintsPtr, Info));

    assert(CDecl != nullptr);
    assert(CDefn != nullptr);
    bool AnyConstrained;
    if (CDecl->numParams() == CDefn->numParams()) {
      // Compare parameters.
      for (unsigned i = 0; i < CDecl->numParams(); ++i) {
        auto Decl = getHighest(CDecl->getParamVar(i), Info);
        auto Defn = getHighest(CDefn->getParamVar(i), Info);
        assert(Decl);
        assert(Defn);

        // If this holds, then we want to insert a bounds safe interface.
        AnyConstrained =
            Defn->anyChanges(Info.getConstraints().getVariables());
        // Definition is more precise than declaration so declaration
        // has to be WILD.
        if (AnyConstrained && Decl->hasWild(CS.getVariables()) &&
            updateDeclWithDefnType(Decl, Defn, Info)) {
          NumITypeVars++;
        }
      }

    }


    // Compare returns.
    auto Decl = getHighest(CDecl->getReturnVars(), Info);
    auto Defn = getHighest(CDefn->getReturnVars(), Info);


    AnyConstrained =
        Defn->anyChanges(Info.getConstraints().getVariables());
    if (AnyConstrained) {
      // Definition is more precise than declaration so declaration
      // has to be WILD.
      if (Decl->hasWild(CS.getVariables()) &&
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
      // Okay, we have constraint variables for declaration there could be a
      // possibility of itypes. Lets save the var atoms.
      updateFunctionConstraintVars(DefKey, CS, DefCVars);
      Ret = true;
    }
  }
  return Ret;
}

