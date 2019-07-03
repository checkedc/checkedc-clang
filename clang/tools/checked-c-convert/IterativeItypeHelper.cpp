//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of ItypeConstraintDetector methods.
//===----------------------------------------------------------------------===//

#include "IterativeItypeHelper.h"

static bool updateDeclWithDefnType(ConstraintVariable *decl, ConstraintVariable *defn,
                                   ProgramInfo &Info) {
  Constraints &CS = Info.getConstraints();
  bool changesHappened = false;
  Constraints::EnvironmentMap &itypeMap = CS.getitypeVarMap();
  PVConstraint *PVDeclCons = dyn_cast<PVConstraint>(decl);
  PVConstraint *PVDefnCons = dyn_cast<PVConstraint>(defn);
  assert(PVDeclCons != nullptr && PVDefnCons != nullptr &&
         "Expected a pointer variable constraint for function parameter but got nullptr");

  ConstAtom *itypeAtom = nullptr;

  for(ConstraintKey k: PVDefnCons->getCvars()) {
    itypeAtom = CS.getVariables()[CS.getVar(k)];
  }
  assert(itypeAtom != nullptr && "Unable to find assignment for definition constraint variable.");
  for(ConstraintKey k: PVDeclCons->getCvars()) {
    VarAtom *cK = CS.getVar(k);
    itypeMap[cK] = itypeAtom;
    changesHappened = true;
  }
  return changesHappened;
}

bool detectAndUpdateITypeVars(ProgramInfo &Info) {
  Constraints &CS = Info.getConstraints();
  bool changedHappened = false;
  for(auto &funDefCon: CS.getFuncDefnVarMap()) {
    std::string funcName = funDefCon.first;
    FVConstraint *cDefn = dyn_cast<FVConstraint>(getHighest(funDefCon.second, Info));

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
        if (anyConstrained && Defn->isLt(*Decl, Info)) {
          updateDeclWithDefnType(Decl, Defn, Info);
          changedHappened = true;
        }
      }

    }

    // Compare returns.
    auto Decl = getHighest(cDecl->getReturnVars(), Info);
    auto Defn = getHighest(cDefn->getReturnVars(), Info);


    bool anyConstrained = Defn->anyChanges(Info.getConstraints().getVariables());
    if(anyConstrained) {
      // definition is more precise than declaration.
      // Section 5.3:
      // https://www.microsoft.com/en-us/research/uploads/prod/2019/05/checkedc-post2019.pdf
      if (Defn->isLt(*Decl, Info)) {
        updateDeclWithDefnType(Decl, Defn, Info);
        changedHappened = true;
      }
    }
  }
  return changedHappened;
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

  // Get constraint variables for the declaration and the definition.
  // Those constraints should be function constraints.
  if(Declaration == nullptr) {
    // if there is no declaration?
    // get the on demand function variable constraint.
    CS.getFuncDeclVarMap()[funcName] = Info.getOnDemandFuncDeclarationConstraint(Definition, Context);
  } else {
    CS.getFuncDeclVarMap()[funcName] = Info.getVariableOnDemand(Declaration, Context, false);
  }

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

