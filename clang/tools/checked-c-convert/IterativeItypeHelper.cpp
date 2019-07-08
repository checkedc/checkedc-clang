//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of ItypeConstraintDetector methods.
//===----------------------------------------------------------------------===//

#include "IterativeItypeHelper.h"

// map that stored the newly detected itype parameters and
// returns that are detected in this iteration.
static Constraints::EnvironmentMap currIterationItypeMap;
// map that contains the constraint variables of parameters
// and its return for all functions (including their declarations and definitions).
// this map is used to determine new detection of itypes.
static std::map<std::string, std::map<VarAtom*, ConstAtom*>> funcParamsReturnSavedValues;

// this method saves the constraint vars of parameters and return of all the provided
// FVConstraint vars with default value as null.
// these are used to later check if anything has changed, in which case the corresponding
// function will be considered as modified.
static void updateFunctionConstraintVars(std::string funcName, Constraints &CS, std::set<ConstraintVariable*> &fvconstraintVars) {
  for(auto fvcvar: fvconstraintVars) {
    assert(dyn_cast<FVConstraint>(fvcvar) && "Expected a function variable.");
    FVConstraint *fvCons = dyn_cast<FVConstraint>(fvcvar);
    // update the variables of function parameters
    for(unsigned i=0; i < fvCons->numParams(); i++) {
      for(ConstraintVariable *paramVar: fvCons->getParamVar(i)) {
        assert(dyn_cast<PVConstraint>(paramVar) && "Expected a pointer variable constraint.");
        PVConstraint *pvConst = dyn_cast<PVConstraint>(paramVar);
        for(auto cVar: pvConst->getCvars()) {
          VarAtom *currVarAtom = CS.getVar(cVar);
          // default value is null
          funcParamsReturnSavedValues[funcName][currVarAtom] = nullptr;
        }
      }
    }
    // update the variables of function return vars.
    for(ConstraintVariable *returnVar: fvCons->getReturnVars()) {
      assert(dyn_cast<PVConstraint>(returnVar) && "Expected a pointer variable constraint.");
      PVConstraint *retVarConst = dyn_cast<PVConstraint>(returnVar);
      for(auto cVar: retVarConst->getCvars()) {
        VarAtom *currVarAtom = CS.getVar(cVar);
        // the default value is null.
        funcParamsReturnSavedValues[funcName][currVarAtom] = nullptr;
      }
    }
  }
}

bool identifyModifiedFunctions(Constraints &CS, std::set<std::string> &modifiedFunctions) {
  modifiedFunctions.clear();
  // get the current values.
  Constraints::EnvironmentMap &currEnvMap = CS.getVariables();
  // check to see if they differ from previous values
  for (auto &prevFuncVals: funcParamsReturnSavedValues) {
    std::string funcName = prevFuncVals.first;
    for (auto &currVar: prevFuncVals.second) {
      // check if the value of the constraint variable changed?
      // then we consider the corresponding function as modified.
      if (currEnvMap[currVar.first] != currVar.second) {
        currVar.second = currEnvMap[currVar.first];
        modifiedFunctions.insert(funcName);
      }
    }
  }
  return !modifiedFunctions.empty();
}

unsigned long resetWithitypeConstraints(Constraints &CS) {
  Constraints::EnvironmentMap backupDeclConstraints;
  backupDeclConstraints.clear();
  Constraints::EnvironmentMap &currEnvMap = CS.getVariables();
  unsigned long numConstraintsRemoved = 0;

  // restore the erased constraints.
  // Now, try to remove constraints that
  // depend on ityped constraint variables.
  for (auto &currE: currEnvMap) {
    currE.first->resetErasedConstraints();
    for(auto &currITypeVar: currIterationItypeMap) {
      ConstAtom *targetCons = currITypeVar.second;
      if(!dyn_cast<NTArrAtom>(currITypeVar.second)) {
        targetCons = nullptr;
      }
      if(currE.first->replaceEqConstraint(currITypeVar.first, targetCons)) {
        numConstraintsRemoved++;
      }
    }
  }

  // Check if we removed any constraints?
  if(numConstraintsRemoved > 0) {
    // we removed constraints.
    // Reset everything.

    // backup the computed results of
    // declaration parameters and returns.
    for (auto &currITypeVar: CS.getitypeVarMap()) {
      backupDeclConstraints[currITypeVar.first] = currEnvMap[CS.getVar(currITypeVar.first->getLoc())];
    }

    // reset all constraints to Ptrs.
    CS.resetConstraints();

    // restore the precomputed constraints for declarations.
    for (auto &currITypeVar: backupDeclConstraints) {
      currEnvMap[CS.getVar(currITypeVar.first->getLoc())] = currITypeVar.second;
    }
  }

  return numConstraintsRemoved;

}

// This method updates the pointer type of the declaration constraint variable
// with the type of the definition constraint variable.
static bool updateDeclWithDefnType(ConstraintVariable *decl, ConstraintVariable *defn,
                                   ProgramInfo &Info) {
  Constraints &CS = Info.getConstraints();
  bool changesHappened = false;
  Constraints::EnvironmentMap &itypeMap = CS.getitypeVarMap();
  PVConstraint *PVDeclCons = dyn_cast<PVConstraint>(decl);
  PVConstraint *PVDefnCons = dyn_cast<PVConstraint>(defn);
  assert(PVDeclCons != nullptr && PVDefnCons != nullptr &&
         "Expected a pointer variable constraint for function parameter but got nullptr");

  // check if this is already identified itype
  for(ConstraintKey k: PVDeclCons->getCvars()) {
    VarAtom *cK = CS.getVar(k);
    if (itypeMap.find(cK) != itypeMap.end()) {
        //yes, then no need to do anything.
        return changesHappened;
    }
  }

  ConstAtom *itypeAtom = nullptr;

  for(ConstraintKey k: PVDefnCons->getCvars()) {
    itypeAtom = CS.getVariables()[CS.getVar(k)];
  }
  assert(itypeAtom != nullptr && "Unable to find assignment for definition constraint variable.");
  for(ConstraintKey k: PVDeclCons->getCvars()) {
    VarAtom *cK = CS.getVar(k);
    itypeMap[cK] = itypeAtom;
    currIterationItypeMap[cK] = itypeAtom;
    changesHappened = true;
  }
  return changesHappened;
}

unsigned long detectAndUpdateITypeVars(ProgramInfo &Info, std::set<std::string> &modifiedFunctions) {
  Constraints &CS = Info.getConstraints();
  unsigned long numITypeVars = 0;
  // clear the current iteration itype vars.
  currIterationItypeMap.clear();
  for (auto funcName: modifiedFunctions) {
    FVConstraint *cDefn = dyn_cast<FVConstraint>(getHighest(CS.getFuncDefnVarMap()[funcName], Info));

    FVConstraint *cDecl = dyn_cast<FVConstraint>(getHighest(CS.getFuncDeclVarMap()[funcName], Info));

    assert(cDecl != nullptr);
    assert(cDefn != nullptr);

    if (cDecl->numParams() == cDefn->numParams()) {
      // Compare parameters.
      for (unsigned i = 0; i < cDecl->numParams(); ++i) {
        auto Decl = getHighest(cDecl->getParamVar(i), Info);
        auto Defn = getHighest(cDefn->getParamVar(i), Info);
        assert(Decl);
        assert(Defn);

        // If this holds, then we want to insert a bounds safe interface.
        bool anyConstrained = Defn->anyChanges(Info.getConstraints().getVariables());
        // definition is more precise than declaration.
        // Section 5.3:
        // https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf
        if (anyConstrained && Defn->isLt(*Decl, Info) && updateDeclWithDefnType(Decl, Defn, Info)) {
          numITypeVars++;
        }
      }

    }


    // Compare returns.
    auto Decl = getHighest(cDecl->getReturnVars(), Info);
    auto Defn = getHighest(cDefn->getReturnVars(), Info);


    bool anyConstrained = Defn->anyChanges(Info.getConstraints().getVariables());
    if (anyConstrained) {
      // definition is more precise than declaration.
      // Section 5.3:
      // https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf
      if (Defn->isLt(*Decl, Info) && updateDeclWithDefnType(Decl, Defn, Info)) {
        numITypeVars++;
      }
    }
  }
  return numITypeVars;
}

bool FVConstraintDetectorVisitor::VisitFunctionDecl(FunctionDecl *FD) {

  auto funcName = FD->getNameAsString();

  // Make sure we haven't visited this function name before, and that we
  // only visit it once.
  if (VisitedSet.find(funcName) != VisitedSet.end())
    return true;
  else
    VisitedSet.insert(funcName);

  // Do we have a definition for this declaration?
  FunctionDecl *Definition = getDefinition(FD);
  FunctionDecl *Declaration = getDeclaration(FD);

  if(Definition == nullptr)
    return true;

  Constraints &CS = Info.getConstraints();

  CS.getFuncDefnVarMap()[funcName] = Info.getVariableOnDemand(Definition, Context, true);
  // save the constraint vars of parameters and return of the definition.
  updateFunctionConstraintVars(funcName, CS, CS.getFuncDefnVarMap()[funcName]);

  // Get constraint variables for the declaration and the definition.
  // Those constraints should be function constraints.
  if(Declaration == nullptr) {
    // if there is no declaration?
    // get the on demand function variable constraint.
    CS.getFuncDeclVarMap()[funcName] = Info.getOnDemandFuncDeclarationConstraint(Definition, Context);
  } else {
    CS.getFuncDeclVarMap()[funcName] = Info.getVariableOnDemand(Declaration, Context, false);
  }
  // save the constraint vars of parameters and return of the declaration.
  updateFunctionConstraintVars(funcName, CS, CS.getFuncDeclVarMap()[funcName]);

  return true;
}

void FVConstraintDetectorConsumer::HandleTranslationUnit(ASTContext &C) {
  Info.enterCompilationUnit(C);

  std::set<std::string> v;
  FVConstraintDetectorVisitor CPV = FVConstraintDetectorVisitor(&C, Info, v);
  for (const auto &D : C.getTranslationUnitDecl()->decls())
    CPV.TraverseDecl(D);

  Info.exitCompilationUnit();
  return;
}

