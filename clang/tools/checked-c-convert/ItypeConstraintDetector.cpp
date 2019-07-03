//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of ItypeConstraintDetector methods.
//===----------------------------------------------------------------------===//

#include "ItypeConstraintDetector.h"

class ItypeDetectorVisitor : public RecursiveASTVisitor<ItypeDetectorVisitor> {
public:
  explicit ItypeDetectorVisitor(ASTContext *C, ProgramInfo &I, std::set<std::string> &V)
    : Context(C), Info(I), VisitedSet(V) {}

  bool VisitFunctionDecl(FunctionDecl *);
private:
  bool updateDeclWithDefnType(ConstraintVariable *decl, ConstraintVariable *defn);
  ASTContext            *Context;
  ProgramInfo           &Info;
  std::set<std::string> &VisitedSet;
};

bool ItypeDetectorVisitor::updateDeclWithDefnType(ConstraintVariable *decl,
                                                  ConstraintVariable *defn) {
  Constraints &CS = Info.getConstraints();
  Constraints::EnvironmentMap &itypeMap = CS.getitypeVarMap();
  PVConstraint *PVDeclCons = dyn_cast<PVConstraint>(decl);
  PVConstraint *PVDefnCons = dyn_cast<PVConstraint>(defn);
  assert(PVDeclCons != nullptr && PVDefnCons != nullptr &&
         "Expected a pointer variable constraint for function paramter but got nullptr");

  ConstAtom *itypeAtom = nullptr;

  for(ConstraintKey k: PVDefnCons->getCvars()) {
    itypeAtom = CS.getVariables()[CS.getVar(k)];
  }
  for(ConstraintKey k: PVDeclCons->getCvars()) {
    VarAtom *cK = CS.getVar(k);
    itypeMap[cK] = itypeAtom;
  }
}

bool ItypeDetectorVisitor::VisitFunctionDecl(FunctionDecl *FD) {

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

  FVConstraint *cDefn = dyn_cast<FVConstraint>(
    getHighest(Info.getVariableOnDemand(Definition, Context, true), Info));

  FVConstraint *cDecl = nullptr;
  // Get constraint variables for the declaration and the definition.
  // Those constraints should be function constraints.
  if(Declaration == nullptr) {
    // if there is no declaration?
    // get the on demand function variable constraint.
    cDecl = dyn_cast<FVConstraint>(
      getHighest(Info.getOnDemandFuncDeclarationConstraint(Definition, Context), Info));
  } else {
    cDecl = dyn_cast<FVConstraint>(
      getHighest(Info.getVariableOnDemand(Declaration, Context, false), Info));
  }

  assert(cDecl != nullptr);
  assert(cDefn != nullptr);
  Constraints &CS = Info.getConstraints();

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
      if(anyConstrained && Defn->isLt(*Decl, Info)) {
        updateDeclWithDefnType(Decl, Defn);
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
      if(Defn->isLt(*Decl, Info)) {
        updateDeclWithDefnType(Decl, Defn);
      }
    }
  }

  return true;
}

void ItypeDetectorConsumer::HandleTranslationUnit(ASTContext &C) {
  Info.enterCompilationUnit(C);

  std::set<std::string> v;
  ItypeDetectorVisitor CPV = ItypeDetectorVisitor(&C, Info, v);
  for (const auto &D : C.getTranslationUnitDecl()->decls())
    CPV.TraverseDecl(D);

  Info.exitCompilationUnit();
  return;
}

