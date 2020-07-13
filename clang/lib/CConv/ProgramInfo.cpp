//=--ProgramInfo.cpp----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of ProgramInfo methods.
//===----------------------------------------------------------------------===//

#include "clang/CConv/ProgramInfo.h"
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/MappingVisitor.h"
#include <sstream>

using namespace clang;

ProgramInfo::ProgramInfo() :
  persisted(true) {
  ExternalFunctionFVCons.clear();
  StaticFunctionFVCons.clear();
}


void ProgramInfo::merge_MF(ParameterMap &mf) {
  for (auto kv : mf) {
    MF[kv.first] = kv.second;
  }
}


ParameterMap &ProgramInfo::get_MF() {
  return MF;
}

void dumpExtFuncMap(const ProgramInfo::ExternalFunctionMapType &EMap,
                    raw_ostream &O) {
  for (const auto &DefM : EMap) {
    O << "Func Name:" << DefM.first << " => ";
    for (const auto J : DefM.second) {
      O << "[ ";
      J->print(O);
      O << " ]\n";
    }
    O << "\n";
  }
}

void dumpStaticFuncMap(const ProgramInfo::StaticFunctionMapType &EMap,
                       raw_ostream &O) {
  for (const auto &DefM : EMap) {
    O << "File Name:" << DefM.first << " => ";
    for (const auto &Tmp : DefM.second) {
      O << " Func Name:"<< Tmp.first << " => \n";
      for (const auto J : Tmp.second) {
        O << "[ ";
        J->print(O);
        O << "]\n";
      }
      O << "\n";
    }
    O << "\n";
  }
}

void dumpExtFuncMapJson(const ProgramInfo::ExternalFunctionMapType &EMap,
                        raw_ostream &O) {
  bool AddComma = false;
  for (const auto &DefM : EMap) {
    if (AddComma) {
      O << ",\n";
    }
    O << "{\"FuncName\":\"" << DefM.first << "\", \"Constraints\":[";
    bool AddComma1 = false;
    for (const auto J : DefM.second) {
      if (AddComma1) {
        O << ",";
      }
      J->dump_json(O);
      AddComma1 = true;
    }
    O << "]}";
    AddComma = true;
  }
}

void dumpStaticFuncMapJson(const ProgramInfo::StaticFunctionMapType &EMap,
                           raw_ostream &O) {
  bool AddComma = false;
  for (const auto &DefM : EMap) {
    if (AddComma) {
      O << ",\n";
    }
    O << "{\"FuncName\":\"" << DefM.first << "\", \"Constraints\":[";
    bool AddComma1 = false;
    for (const auto J : DefM.second) {
      if (AddComma1) {
        O << ",";
      }
      O << "{\"FileName\":\"" << J.first << "\", \"FVConstraints\":[";
      bool AddComma2 = false;
      for (const auto FV : J.second) {
        if (AddComma2) {
          O << ",";
        }
        FV->dump_json(O);
        AddComma2 = true;
      }
      O << "]}\n";
      AddComma1 = true;
    }
    O << "]}";
    AddComma = true;
  }
}


void ProgramInfo::print(raw_ostream &O) const {
  CS.print(O);
  O << "\n";

  O << "Constraint Variables\n";
  for ( const auto &I : Variables ) {
    PersistentSourceLoc L = I.first;
    const CVarSet &S = I.second;
    L.print(O);
    O << "=>";
    for (const auto &J : S) {
      O << "[ ";
      J->print(O);
      O << " ]";
    }
    O << "\n";
  }

  O << "External Function Definitions\n";
  dumpExtFuncMap(ExternalFunctionFVCons, O);
  O << "Static Function Definitions\n";
  dumpStaticFuncMap(StaticFunctionFVCons, O);
}

void ProgramInfo::dump_json(llvm::raw_ostream &O) const {
  O << "{\"Setup\":";
  CS.dump_json(O);
  // Dump the constraint variables.
  O << ", \"ConstraintVariables\":[";
  bool AddComma = false;
  for ( const auto &I : Variables ) {
    if (AddComma) {
      O << ",\n";
    }
    PersistentSourceLoc L = I.first;
    const CVarSet &S = I.second;

    O << "{\"line\":\"";
    L.print(O);
    O << "\",";
    O << "\"Variables\":[";
    bool AddComma1 = false;
    for (const auto &J : S) {
      if (AddComma1) {
        O << ",";
      }
      J->dump_json(O);
      AddComma1 = true;
    }
    O << "]";
    O << "}";
    AddComma = true;
  }
  O << "]";
  O << ", \"ExternalFunctionDefinitions\":[";
  dumpExtFuncMapJson(ExternalFunctionFVCons, O);
  O << "], \"StaticFunctionDefinitions\":[";
  dumpStaticFuncMapJson(StaticFunctionFVCons, O);
  O << "]}";
}

// Given a ConstraintVariable V, retrieve all of the unique
// constraint variables used by V. If V is just a 
// PointerVariableConstraint, then this is just the contents 
// of 'vars'. If it either has a function pointer, or V is
// a function, then recurses on the return and parameter
// constraints.
static
CAtoms getVarsFromConstraint(ConstraintVariable *V, CAtoms T) {
  CAtoms R = T;

  if (PVConstraint *PVC = dyn_cast<PVConstraint>(V)) {
    R.insert(R.begin(), PVC->getCvars().begin(), PVC->getCvars().end());
   if (FVConstraint *FVC = PVC->getFV()) 
     return getVarsFromConstraint(FVC, R);
  } else if (FVConstraint *FVC = dyn_cast<FVConstraint>(V)) {
    for (const auto &C : FVC->getReturnVars()) {
      CAtoms tmp = getVarsFromConstraint(C, R);
      R.insert(R.begin(), tmp.begin(), tmp.end());
    }
    for (unsigned i = 0; i < FVC->numParams(); i++) {
      for (const auto &C : FVC->getParamVar(i)) {
        CAtoms tmp = getVarsFromConstraint(C, R);
        R.insert(R.begin(), tmp.begin(), tmp.end());
      }
    }
  }

  return R;
}

// Print out statistics of constraint variables on a per-file basis.
void ProgramInfo::print_stats(std::set<std::string> &F, raw_ostream &O,
                              bool OnlySummary) {
  if (!OnlySummary) {
    O << "Enable itype propagation:" << EnablePropThruIType << "\n";
    O << "Sound handling of var args functions:" << HandleVARARGS << "\n";
  }
  std::map<std::string, std::tuple<int, int, int, int, int>> FilesToVars;
  EnvironmentMap Env = CS.getVariables();
  unsigned int totC, totP, totNt, totA, totWi;
  totC = totP = totNt = totA = totWi = 0;

  // First, build the map and perform the aggregation.
  for (auto &I : Variables) {
    std::string FileName = I.first.getFileName();
    if (F.count(FileName)) {
      int varC = 0;
      int pC = 0;
      int ntAC = 0;
      int aC = 0;
      int wC = 0;

      auto J = FilesToVars.find(FileName);
      if (J != FilesToVars.end())
        std::tie(varC, pC, ntAC, aC, wC) = J->second;

      CAtoms FoundVars;
      for (auto &C : I.second) {
        CAtoms tmp = getVarsFromConstraint(C, FoundVars);
        FoundVars.insert(FoundVars.begin(), tmp.begin(), tmp.end());
      }

      varC += FoundVars.size();
      for (const auto &N : FoundVars) {
        ConstAtom *CA = CS.getAssignment(N);
        switch (CA->getKind()) {
          case Atom::A_Arr:
            aC += 1;
            break;
          case Atom::A_NTArr:
            ntAC += 1;
            break;
          case Atom::A_Ptr:
            pC += 1;
            break;
          case Atom::A_Wild:
            wC += 1;
            break;
          case Atom::A_Var:
          case Atom::A_Const:
            llvm_unreachable("bad constant in environment map");
        }
      }

      FilesToVars[FileName] = std::tuple<int, int, int, int, int>(varC, pC,
                                                                  ntAC, aC, wC);
    }
  }

  // Then, dump the map to output.
  // if not only summary then dump everything.
  if (!OnlySummary) {
    O << "file|#constraints|#ptr|#ntarr|#arr|#wild\n";
  }
  for (const auto &I : FilesToVars) {
    int v, p, nt, a, w;
    std::tie(v, p, nt, a, w) = I.second;

    totC += v;
    totP += p;
    totNt += nt;
    totA += a;
    totWi += w;
    if (!OnlySummary) {
      O << I.first << "|" << v << "|" << p << "|" << nt << "|" << a << "|" << w;
      O << "\n";
    }
  }

  O << "Summary\nTotalConstraints|TotalPtrs|TotalNTArr|TotalArr|TotalWild\n";
  O << totC << "|" << totP << "|" << totNt << "|" << totA << "|" << totWi << "\n";

  if (AllTypes) {
    ArrBInfo.getBStats().print(O);
  }

}

bool ProgramInfo::isExternOkay(std::string Ext) {
  return llvm::StringSwitch<bool>(Ext)
    .Cases("malloc", "free", true)
    .Default(false);
}

bool ProgramInfo::link() {
  // For every global symbol in all the global symbols that we have found
  // go through and apply rules for whether they are functions or variables.
  if (Verbose)
    llvm::errs() << "Linking!\n";

// MWH: Should never happen: Variables set sizes == 1
  // Multiple Variables can be at the same PersistentSourceLoc. We should
  // constrain that everything that is at the same location is explicitly
  // equal.
//  for (const auto &V : Variables) {
//    std::set<ConstraintVariable *> C = V.second;
//
//    if (C.size() > 1) {
//      assert(false); // should never get here
//      std::set<ConstraintVariable *>::iterator I = C.begin();
//      std::set<ConstraintVariable *>::iterator J = C.begin();
//      ++J;
//
//      while (J != C.end()) {
//        constrainConsVarGeq(*I, *J, CS, nullptr, Same_to_Same, true, this);
//        ++I;
//        ++J;
//      }
//    }
//  }

  // Equate the constraints for all global variables.
  // This is needed for variables that are defined as extern.
  for (const auto &V : GlobalVariableSymbols) {
    const std::set<PVConstraint *> &C = V.second;

    if (C.size() > 1) {
      std::set<PVConstraint *>::iterator I = C.begin();
      std::set<PVConstraint *>::iterator J = C.begin();
      ++J;
      if (Verbose)
        llvm::errs() << "Global variables:" << V.first << "\n";
      while (J != C.end()) {
        constrainConsVarGeq(*I, *J, CS, nullptr, Same_to_Same, true, this);
        ++I;
        ++J;
      }
    }
  }

  for (const auto &V : ExternGVars) {
      // if a definition for this global variable has not been seen,
      // constrain everything about it
      if(!V.second) {
          std::string VarName = V.first;
          std::string Rsn = "External global variable " + VarName + " has no definition";
          const std::set<PVConstraint *> &C = GlobalVariableSymbols[VarName];
          for(const auto &Var : C) {
              Var->constrainToWild(CS, Rsn);
          }
      }
  }

  // For every global function that is an unresolved external, constrain 
  // its parameter types to be wild. Unless it has a bounds-safe annotation. 
  for (const auto &U : ExternFunctions) {
    // If we've seen this symbol, but never seen a body for it, constrain
    // everything about it.
    if (!U.second && !isExternOkay(U.first)) {
      // Some global symbols we don't need to constrain to wild, like 
      // malloc and free. Check those here and skip if we find them. 
      std::string FuncName = U.first;
      auto FuncDeclFVIterator = ExternalFunctionFVCons.find(FuncName);
      assert(FuncDeclFVIterator != ExternalFunctionFVCons.end());
      const std::set<FVConstraint *> &Gs = (*FuncDeclFVIterator).second;

      for (const auto GIterator : Gs) {
        auto G = GIterator;
        for (const auto &R : G->getReturnVars()) {
          std::string Rsn = "Return value of an external function:" + FuncName;
          R->constrainToWild(CS, Rsn);
        }
        std::string rsn = "Inner pointer of a parameter to external function.";
        for (unsigned i = 0; i < G->numParams(); i++)
          for (const auto &PVar : G->getParamVar(i))
            PVar->constrainToWild(CS, rsn);
      }
    }
  }

  return true;
}

bool ProgramInfo::isAnExternFunction(const std::string &FName) {
  return !ExternFunctions[FName];
}

// Populate Variables, VarDeclToStatement, RVariables, and DepthMap with
// AST data structures that correspond do the data stored in PDMap and
// ReversePDMap.
void ProgramInfo::enterCompilationUnit(ASTContext &Context) {
  assert(persisted);
  // Get a set of all of the PersistentSourceLoc's we need to fill in.
  std::set<PersistentSourceLoc> P;
  //for (auto I : PersistentVariables)
  //  P.insert(I.first);

  // Resolve the PersistentSourceLoc to one of Decl,Stmt,Type.
  MappingVisitor V(P, Context);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  for (const auto &D : TUD->decls())
    V.TraverseDecl(D);

  persisted = false;
  return;
}

// Remove any references we maintain to AST data structure pointers.
// After this, the Variables, VarDeclToStatement, RVariables, and DepthMap
// should all be empty.
void ProgramInfo::exitCompilationUnit() {
  assert(!persisted);
  persisted = true;
  return;
}

bool
ProgramInfo::insertIntoExternalFunctionMap(ExternalFunctionMapType &Map,
                                           const std::string &FuncName,
                                           std::set<FVConstraint *> &ToIns) {
  bool RetVal = false;
  if (Map.find(FuncName) == Map.end()) {
    Map[FuncName] = ToIns;
    RetVal = true;
  } else {
    auto oldS = Map[FuncName];
    auto *newC = getOnly(ToIns);
    auto *oldC = getOnly(oldS);
    bool isDef = newC->hasBody();
    if (isDef) {
      newC->brainTransplant(oldC);
      Map[FuncName] = ToIns;
      RetVal = true;
    } else if (!oldC->hasBody()) {
      // if the current FV constraint is not a definition?
      // then merge.
      oldC->mergeDeclaration(newC);
    }
  }
  return RetVal;
}

bool
ProgramInfo::insertIntoStaticFunctionMap(StaticFunctionMapType &Map,
                                         const std::string &FuncName,
                                         const std::string &FileName,
                                         std::set<FVConstraint *> &ToIns) {
  bool RetVal = false;
  if (Map.find(FileName) == Map.end()) {
    Map[FileName][FuncName] = ToIns;
    RetVal = true;
  } else {
    RetVal = insertIntoExternalFunctionMap(Map[FileName],FuncName,ToIns);
  }
  return RetVal;
}

bool
ProgramInfo::insertNewFVConstraints(FunctionDecl *FD,
                                   std::set<FVConstraint *> &FVcons,
                                   ASTContext *C) {
  bool ret = false;
  std::string FuncName = FD->getNameAsString();
  if (FD->isGlobal()) {
    // external method.
    ret = insertIntoExternalFunctionMap(ExternalFunctionFVCons,
                                        FuncName, FVcons);
    bool isDef = getOnly(FVcons)->hasBody();
    if (isDef) {
      ExternFunctions[FuncName] = true;
    } else {
      if (!ExternFunctions[FuncName])
        ExternFunctions[FuncName] = false;
    }
  } else {
    // static method
    auto Psl = PersistentSourceLoc::mkPSL(FD, *C);
    std::string FuncFileName = Psl.getFileName();
      ret = insertIntoStaticFunctionMap(StaticFunctionFVCons, FuncName,
                                        FuncFileName, FVcons);
  }
  return ret;
}

void ProgramInfo::specialCaseVarIntros(ValueDecl *D, ASTContext *Context) {
  // Special-case for va_list, constrain to wild.
  if (isVarArgType(D->getType().getAsString()) || hasVoidType(D)) {
    // set the reason for making this variable WILD.
    std::string Rsn = "Variable type void.";
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(D, *Context);
    if (!D->getType()->isVoidType())
      Rsn = "Variable type is va_list.";
    for (const auto &I : getVariable(D, Context)) {
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(I)) {
        PVC->constrainToWild(CS, Rsn, &PL);
      }
    }
  }
}

// For each pointer type in the declaration of D, add a variable to the
// constraint system for that pointer type.
void ProgramInfo::addVariable(clang::DeclaratorDecl *D,
                              clang::ASTContext *astContext) {
  assert(!persisted);

  PersistentSourceLoc PLoc = PersistentSourceLoc::mkPSL(D, *astContext);
  assert(PLoc.valid());

  // We only add a PVConstraint or an FVConstraint if the set at
  // Variables[PLoc] does not contain one already. TODO: Explain why would this happen
  CVarSet &S = Variables[PLoc];
  if (S.size()) return;

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    // Function Decls have FVConstraints.
    FVConstraint *F = new FVConstraint(D, *this, *astContext);
    std::set<FVConstraint *> NewFVars;
    /* Store the FVConstraint in the global and Variables maps */
    NewFVars.insert(F);
    insertNewFVConstraints(FD, NewFVars, astContext);
    S.insert(F);
    // Add mappings from the parameters PLoc to the constraint variables for
    // the parameters.
    for (unsigned i = 0; i < FD->getNumParams(); i++) {
      ParmVarDecl *PVD = FD->getParamDecl(i);
      CVarSet PS = F->getParamVar(i);
      assert(PS.size());
      PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(PVD, *astContext);
      Variables[PSL].insert(PS.begin(), PS.end());
      specialCaseVarIntros(PVD, astContext);
    }

  } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    const Type *Ty = VD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    if (Ty->isPointerType() || Ty->isArrayType()) {
      PVConstraint *P = new PVConstraint(D, *this, *astContext);
      S.insert(P);
      std::string VarName = VD->getName();
      if (VD->hasGlobalStorage()) {
          // if we see a definition for this global variable, indicate so in ExternGVars
          if(VD->hasDefinition() || VD->hasDefinition(*astContext)) {
              ExternGVars[VarName] = true;
          }
          // if we don't, check that we haven't seen one before before setting to false
          else if(!ExternGVars[VarName]) {
              ExternGVars[VarName] = false;
          }
          GlobalVariableSymbols[VarName].insert(P);
      }
      specialCaseVarIntros(D, astContext);
    }

  } else if (FieldDecl *FlD = dyn_cast<FieldDecl>(D)) {
    const Type *Ty = FlD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    if (Ty->isPointerType() || Ty->isArrayType()) {
      PVConstraint *P = new PVConstraint(D, *this, *astContext);
      S.insert(P);
      specialCaseVarIntros(D, astContext);
    }
  } else
    llvm_unreachable("unknown decl type");

  constrainWildIfMacro(S, D->getLocation());
}

CVarSet
    &ProgramInfo::getPersistentConstraintVars(Expr *E,
                                              clang::ASTContext *AstContext){
  PersistentSourceLoc PLoc = PersistentSourceLoc::mkPSL(E, *AstContext);
  assert(PLoc.valid());

  return Variables[PLoc];
}

// The Rewriter won't let us re-write things that are in macros. So, we
// should check to see if what we just added was defined within a macro.
// If it was, we should constrain it to top. This is sad. Hopefully,
// someday, the Rewriter will become less lame and let us re-write stuff
// in macros.
void ProgramInfo::constrainWildIfMacro(CVarSet S,
                                       SourceLocation Location) {
  std::string Rsn = "Pointer in Macro declaration.";
  if (!Rewriter::isRewritable(Location))
    for (const auto &C : S)
      C->constrainToWild(CS, Rsn);
}

//std::string ProgramInfo::getUniqueDeclKey(Decl *D, ASTContext *C) {
//  auto Psl = PersistentSourceLoc::mkPSL(D, *C);
//  std::string FileName = Psl.getFileName() + ":" +
//                         std::to_string(Psl.getLineNo());
//  std::string Dname = D->getDeclKindName();
//  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
//    Dname = FD->getNameAsString();
//  }
//  std::string DeclKey = FileName + ":" + Dname;
//  return DeclKey;
//}
//
//std::string ProgramInfo::getUniqueFuncKey(FunctionDecl *D,
//                                          ASTContext *C) {
//  // Get unique key for a function: which is function name,
//  // file and line number.
//  if (FunctionDecl *FuncDef = getDefinition(D)) {
//    D = FuncDef;
//  }
//  return getUniqueDeclKey(D, C);
//}

std::set<FVConstraint *> *
ProgramInfo::getFuncConstraints(FunctionDecl *D, ASTContext *C) {

  std::string FuncName = D->getNameAsString();
  if (D->isGlobal()) {
    // Is this a global (externally visible) function?
    if (ExternalFunctionFVCons.find(FuncName) != ExternalFunctionFVCons.end()) {
      return &ExternalFunctionFVCons[FuncName];
    }
  } else {
    // Static function.
    auto Psl = PersistentSourceLoc::mkPSL(D, *C);
    std::string FileName = Psl.getFileName();
    if (StaticFunctionFVCons.find(FileName) != StaticFunctionFVCons.end() &&
        StaticFunctionFVCons[FileName].find(FuncName) !=
            StaticFunctionFVCons[FileName].end()) {
      return &StaticFunctionFVCons[FileName][FuncName];
    }
  }
  return nullptr;
}

std::set<FVConstraint *> *ProgramInfo::getFuncFVConstraints(FunctionDecl *FD,
                                                            ASTContext *C) {
  std::string FuncName = FD->getNameAsString();
  std::set<FVConstraint *> *FunFVars = nullptr;

  if (FD->isGlobal()) {
    FunFVars = getExtFuncDefnConstraintSet(FuncName);
    // FIXME: We are being asked to access a function never declared; best action?
    if (FunFVars == nullptr) {
      // make one
      FVConstraint *F = new FVConstraint(FD, *this, *C);
      assert(!F->hasBody());
      ExternalFunctionFVCons[FuncName].insert(F);
      FunFVars = &ExternalFunctionFVCons[FuncName];
    }
  } else {
    auto Psl = PersistentSourceLoc::mkPSL(FD, *C);
    std::string FileName = Psl.getFileName();
    FunFVars = getStaticFuncConstraintSet(FuncName, FileName);
  }

  return FunFVars;
}

// Given a decl, return the variables for the constraints of the Decl.
CVarSet ProgramInfo::getVariable(clang::Decl *D, clang::ASTContext *C) {
  assert(!persisted);

  if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
    int PIdx = -1;
    DeclContext *DC = PD->getParentFunctionOrMethod();
    // This can fail for extern definitions
    if(!DC)
      return std::set<ConstraintVariable*>();
    FunctionDecl *FD = dyn_cast<FunctionDecl>(DC);
    // Get the parameter index with in the function.
    for (unsigned i = 0; i < FD->getNumParams(); i++) {
      const ParmVarDecl *tmp = FD->getParamDecl(i);
      if (tmp == D) {
        PIdx = i;
        break;
      }
    }
    // Get corresponding FVConstraint vars.
    std::set<FVConstraint *> *FunFVars = getFuncFVConstraints(FD, C);
    assert(FunFVars != nullptr && "Unable to find function constraints.");
    CVarSet ParameterCons;
    ParameterCons.clear();
    for (auto fv : *FunFVars) {
      auto currParamConstraint = fv->getParamVar(PIdx);
      ParameterCons.insert(currParamConstraint.begin(),
                           currParamConstraint.end());
    }
    return ParameterCons;

  } else if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    std::set<FVConstraint *> *FunFVars = getFuncFVConstraints(FD, C);
    if (FunFVars == nullptr) {
      llvm::errs() << "No fun constraints for " << FD->getName() << "?!\n";
    }
    assert (FunFVars != nullptr && "Unable to find function constraints.");
    std::set<ConstraintVariable*> TmpRet;
    TmpRet.insert(FunFVars->begin(), FunFVars->end());
    return TmpRet;

  } else /* neither function nor function parameter */ {
    VariableMap::iterator I =
        Variables.find(PersistentSourceLoc::mkPSL(D, *C));
    if (I != Variables.end()) {
      return I->second;
    }
    return CVarSet();
  }
}

std::set<FVConstraint *> *
    ProgramInfo::getExtFuncDefnConstraintSet(std::string FuncName) {
  if (ExternalFunctionFVCons.find(FuncName) != ExternalFunctionFVCons.end()) {
    return &(ExternalFunctionFVCons[FuncName]);
  }
  return nullptr;
}

std::set<FVConstraint *> *
ProgramInfo::getStaticFuncConstraintSet(std::string FuncName,
                                            std::string FileName) {
  if (StaticFunctionFVCons.find(FileName) != StaticFunctionFVCons.end() &&
      StaticFunctionFVCons[FileName].find(FuncName) !=
          StaticFunctionFVCons[FileName].end()) {
    return &(StaticFunctionFVCons[FileName][FuncName]);
  }
  return nullptr;
}

bool ProgramInfo::computePointerDisjointSet() {
  ConstraintDisjointSet.Clear();
  CVars WildPtrs;
  WildPtrs.clear();
  auto &WildPtrsReason = ConstraintDisjointSet.RealWildPtrsWithReasons;
  auto &CurrLeaders = ConstraintDisjointSet.Leaders;
  auto &CurrGroups = ConstraintDisjointSet.Groups;
  for (auto currC : CS.getConstraints()) {
    if (Geq *EC = dyn_cast<Geq>(currC)) {
      VarAtom *VLhs = dyn_cast<VarAtom>(EC->getLHS());
      if (dyn_cast<WildAtom>(EC->getRHS())) {
        WildPtrsReason[VLhs->getLoc()].WildPtrReason = EC->getReason();
        if (!EC->FileName.empty() && EC->LineNo != 0) {
          WildPtrsReason[VLhs->getLoc()].IsValid = true;
          WildPtrsReason[VLhs->getLoc()].SourceFileName = EC->FileName;
          WildPtrsReason[VLhs->getLoc()].LineNo = EC->LineNo;
          WildPtrsReason[VLhs->getLoc()].ColStart = EC->ColStart;
        }
        WildPtrs.insert(VLhs->getLoc());
      } else {
        VarAtom *Vrhs = dyn_cast<VarAtom>(EC->getRHS());
        if (Vrhs != nullptr)
          ConstraintDisjointSet.AddElements(VLhs->getLoc(), Vrhs->getLoc());
      }
    }
  }

  // Perform adjustment of group leaders. So that, the real-WILD
  // pointers are the leaders for each group.
  for (auto &RealCp : WildPtrsReason) {
    auto &RealCVar = RealCp.first;
    // check if the leader CVar is a real WILD Ptr
    if (CurrLeaders.find(RealCVar) != CurrLeaders.end()) {
      auto OldGroupLeader = CurrLeaders[RealCVar];
      // If not?
      if (ConstraintDisjointSet.RealWildPtrsWithReasons.find(OldGroupLeader) ==
          ConstraintDisjointSet.RealWildPtrsWithReasons.end()) {
        for (auto &LeadersP : CurrLeaders) {
          if (LeadersP.second == OldGroupLeader) {
            LeadersP.second = RealCVar;
          }
        }

        auto &OldG = CurrGroups[OldGroupLeader];
        CurrGroups[RealCVar].insert(OldG.begin(), OldG.end());
        CurrGroups[RealCVar].insert(RealCVar);
        CurrGroups.erase(OldGroupLeader);
      }
    }
  }

  // Compute non-direct WILD pointers.
  for (auto &Gm : CurrGroups) {
    // Is this group a WILD pointer group?
    if (ConstraintDisjointSet.RealWildPtrsWithReasons.find(Gm.first) !=
        ConstraintDisjointSet.RealWildPtrsWithReasons.end()) {
        ConstraintDisjointSet.TotalNonDirectWildPointers.insert(Gm.second.begin(),
                                                              Gm.second.end());
    }
  }

  CVars TmpCKeys;
  TmpCKeys.clear();
  auto &TotalNDirectWPtrs = ConstraintDisjointSet.TotalNonDirectWildPointers;
  // Remove direct WILD pointers from non-direct wild pointers.
  std::set_difference(TotalNDirectWPtrs.begin(), TotalNDirectWPtrs.end(),
                      WildPtrs.begin(), WildPtrs.end(),
                      std::inserter(TmpCKeys, TmpCKeys.begin()));

  // Update the totalNonDirectWildPointers.
  TotalNDirectWPtrs.clear();
  TotalNDirectWPtrs.insert(TmpCKeys.begin(), TmpCKeys.end());

  for ( const auto &I : Variables ) {
    PersistentSourceLoc L = I.first;
    std::string FilePath = L.getFileName();
    if (canWrite(FilePath)) {
      ConstraintDisjointSet.ValidSourceFiles.insert(FilePath);
    } else {
      continue;
    }
    const CVarSet &S = I.second;
    for (auto *CV : S) {
      if (PVConstraint *PV = dyn_cast<PVConstraint>(CV)) {
        for (auto ck : PV->getCvars()) {
          if (VarAtom *VA = dyn_cast<VarAtom>(ck)) {
            ConstraintDisjointSet.PtrSourceMap[VA->getLoc()] =
                (PersistentSourceLoc*)(&(I.first));
          }
        }
      }
      if (FVConstraint *FV = dyn_cast<FVConstraint>(CV)) {
        for (auto PV : FV->getReturnVars()) {
          if (PVConstraint *RPV = dyn_cast<PVConstraint>(PV)) {
            for (auto ck : RPV->getCvars()) {
              if (VarAtom *VA = dyn_cast<VarAtom>(ck)) {
                ConstraintDisjointSet.PtrSourceMap[VA->getLoc()] =
                    (PersistentSourceLoc*)(&(I.first));
              }
            }
          }
        }
      }
    }
  }

  // Compute all the WILD pointers.
  CVars WildCkeys;
  for (auto &G : CurrGroups) {
    WildCkeys.clear();
    std::set_intersection(G.second.begin(), G.second.end(), WildPtrs.begin(),
                          WildPtrs.end(),
                          std::inserter(WildCkeys, WildCkeys.begin()));

    if (!WildCkeys.empty()) {
      ConstraintDisjointSet.AllWildPtrs.insert(WildCkeys.begin(),
                                               WildCkeys.end());
    }
  }

  return true;
}
