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
#include "clang/CConv/ConstraintsGraph.h"
#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/MappingVisitor.h"
#include "clang/CConv/Utils.h"

#include <sstream>

using namespace clang;

ProgramInfo::ProgramInfo() :
  persisted(true) {
  ExternalFunctionFVCons.clear();
  StaticFunctionFVCons.clear();
}

void dumpExtFuncMap(const ProgramInfo::ExternalFunctionMapType &EMap,
                    raw_ostream &O) {
  for (const auto &DefM : EMap) {
    O << "Func Name:" << DefM.first << " => [ ";
    DefM.second->print(O);
    O << " ]\n";
  }
}

void dumpStaticFuncMap(const ProgramInfo::StaticFunctionMapType &EMap,
                       raw_ostream &O) {
  for (const auto &DefM : EMap) {
    O << "File Name:" << DefM.first << " => ";
    for (const auto &Tmp : DefM.second) {
      O << " Func Name:"<< Tmp.first << " => [ \n";
      Tmp.second->print(O);
      O << " ]\n";
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
    DefM.second->dump_json(O);
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
      J.second->dump_json(O);
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
    L.print(O);
    O << "=>[ ";
    I.second->print(O);
    O << " ]\n";
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

    O << "{\"line\":\"";
    L.print(O);
    O << "\",\"Variables\":[";
    I.second->dump_json(O);
    O << "]}";
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
CAtoms getVarsFromConstraint(ConstraintVariable *V) {
  CAtoms R;
  R.clear();

  if (PVConstraint *PVC = dyn_cast<PVConstraint>(V)) {
    R.insert(R.begin(), PVC->getCvars().begin(), PVC->getCvars().end());
   if (FVConstraint *FVC = PVC->getFV()) 
     return getVarsFromConstraint(FVC);
  } else if (FVConstraint *FVC = dyn_cast<FVConstraint>(V)) {
    if (FVC->getReturnVar()) {
      CAtoms Tmp = getVarsFromConstraint(FVC->getReturnVar());
      R.insert(R.begin(), Tmp.begin(), Tmp.end());
    }
    for (unsigned i = 0; i < FVC->numParams(); i++) {
      CAtoms Tmp = getVarsFromConstraint(FVC->getParamVar(i));
      R.insert(R.begin(), Tmp.begin(), Tmp.end());
    }
  }

  return R;
}

// Print out statistics of constraint variables on a per-file basis.
void ProgramInfo::print_stats(const std::set<std::string> &F, raw_ostream &O,
                              bool OnlySummary, bool JsonFormat) {
  if (!OnlySummary && !JsonFormat) {
    O << "Enable itype propagation:" << EnablePropThruIType << "\n";
    O << "Sound handling of var args functions:" << HandleVARARGS << "\n";
  }
  std::map<std::string, std::tuple<int, int, int, int, int>> FilesToVars;
  CVarSet InSrcCVars;
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

      ConstraintVariable *C = I.second;
      if (C->isForValidDecl()) {
        InSrcCVars.insert(C);
        CAtoms FoundVars = getVarsFromConstraint(C);

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
      }
      FilesToVars[FileName] = std::tuple<int, int, int, int, int>(varC, pC,
                                                                  ntAC, aC, wC);
    }
  }

  // Then, dump the map to output.
  // if not only summary then dump everything.
  if (JsonFormat) {
    O << "{\"Stats\":{";
    O << "\"ConstraintStats\":{";
  }
  if (!OnlySummary) {
    if (JsonFormat) {
      O << "\"Individual\":[";
    } else {
      O << "file|#constraints|#ptr|#ntarr|#arr|#wild\n";
    }
  }
  bool AddComma = false;
  for (const auto &I : FilesToVars) {
    int v, p, nt, a, w;
    std::tie(v, p, nt, a, w) = I.second;

    totC += v;
    totP += p;
    totNt += nt;
    totA += a;
    totWi += w;
    if (!OnlySummary) {
      if (JsonFormat) {
        if (AddComma) {
          O << ",\n";
        }
        O << "{\"" << I.first << "\":{";
        O << "\"constraints\":" << v << ",";
        O << "\"ptr\":" << p << ",";
        O << "\"ntarr\":" << nt << ",";
        O << "\"arr\":" << a << ",";
        O << "\"wild\":" << w;
        O << "}}";
        AddComma = true;
      } else {
        O << I.first << "|" << v << "|" << p << "|" << nt << "|" << a << "|"
          << w;
        O << "\n";
      }
    }
  }
  if (!OnlySummary && JsonFormat) {
    O << "],";
  }

  if (!JsonFormat) {
    O << "Summary\nTotalConstraints|TotalPtrs|TotalNTArr|TotalArr|TotalWild\n";
    O << totC << "|" << totP << "|" << totNt << "|" << totA << "|" << totWi
      << "\n";
  } else {
    O << "\"Summary\":{";
    O << "\"TotalConstraints\":" << totC << ",";
    O << "\"TotalPtrs\":" << totP << ",";
    O << "\"TotalNTArr\":" << totNt << ",";
    O << "\"TotalArr\":" << totA << ",";
    O << "\"TotalWild\":" << totWi;
    O << "}},\n";
  }

  if (AllTypes) {
    if (JsonFormat) {
      O << "\"BoundsStats\":";
    }
    ArrBInfo.print_stats(O, InSrcCVars, JsonFormat);
  }

  if (JsonFormat) {
    O << "}}";
  }
}

bool ProgramInfo::isExternOkay(const std::string &Ext) {
  return llvm::StringSwitch<bool>(Ext)
    .Cases("malloc", "free", true)
    .Default(false);
}

bool ProgramInfo::link() {
  // For every global symbol in all the global symbols that we have found
  // go through and apply rules for whether they are functions or variables.
  if (Verbose)
    llvm::errs() << "Linking!\n";

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
    std::string FuncName = U.first;
    if (!U.second && !isExternOkay(FuncName)) {
      // Some global symbols we don't need to constrain to wild, like 
      // malloc and free. Check those here and skip if we find them. 
      FVConstraint *G = getExtFuncDefnConstraint(FuncName);
      assert("Function constraints could not be found!" && G != nullptr);

      // If there was a checked type on a variable in the input program, it
      // should stay that way. Otherwise, we shouldn't be adding a checked type
      // to an extern function.
      if (!G->getReturnVar()->getIsOriginallyChecked()) {
        std::string Rsn = "Return value of an external function:" + FuncName;
        G->getReturnVar()->constrainToWild(CS, Rsn);
      }

      std::string rsn = "Inner pointer of a parameter to external function.";
      for (unsigned i = 0; i < G->numParams(); i++)
        if (!G->getParamVar(i)->getIsOriginallyChecked())
          G->getParamVar(i)->constrainToWild(CS, rsn);
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
                                           FVConstraint *newC) {
  bool RetVal = false;
  if (Map.find(FuncName) == Map.end()) {
    Map[FuncName] = newC;
    RetVal = true;
  } else {
    auto *oldC = Map[FuncName];
    if (!oldC->hasBody()) {
      if (newC->hasBody() ||
          (oldC->numParams() == 0 && newC->numParams() != 0)) {
        newC->brainTransplant(oldC, *this);
        Map[FuncName] = newC;
        RetVal = true;
      } else
        // if the current FV constraint is not a definition?
        // then merge.
        oldC->mergeDeclaration(newC, *this);
    } else if (newC->hasBody())
      llvm::errs() << "Warning: duplicate definition " << FuncName << "\n";
    // else oldc->hasBody() so we can ignore the newC prototype
  }
  return RetVal;
}

bool ProgramInfo::insertIntoStaticFunctionMap (StaticFunctionMapType &Map,
                                               const std::string &FuncName,
                                               const std::string &FileName,
                                               FVConstraint *ToIns) {
  bool RetVal = false;
  if (Map.find(FileName) == Map.end()) {
    Map[FileName][FuncName] = ToIns;
    RetVal = true;
  } else {
    RetVal = insertIntoExternalFunctionMap(Map[FileName],FuncName,ToIns);
  }
  return RetVal;
}

bool ProgramInfo::insertNewFVConstraint(FunctionDecl *FD, FVConstraint *FVCon,
                                        ASTContext *C) {
  bool ret = false;
  std::string FuncName = FD->getNameAsString();
  if (FD->isGlobal()) {
    // external method.
    ret = insertIntoExternalFunctionMap(ExternalFunctionFVCons,
                                        FuncName, FVCon);
    bool isDef = FVCon->hasBody();
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
                                      FuncFileName, FVCon);
  }
  return ret;
}

void ProgramInfo::specialCaseVarIntros(ValueDecl *D, ASTContext *Context) {
  // Special-case for va_list, constrain to wild.
  bool IsGeneric = false;
  if (auto *PVD = dyn_cast<ParmVarDecl>(D))
    IsGeneric = getTypeVariableType(PVD) != nullptr;
  if (isVarArgType(D->getType().getAsString()) ||
      (hasVoidType(D) && !IsGeneric)) {
    // set the reason for making this variable WILD.
    std::string Rsn = "Variable type void.";
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(D, *Context);
    if (!D->getType()->isVoidType())
      Rsn = "Variable type is va_list.";
    CVarOption CVOpt = getVariable(D, Context);
    if (CVOpt.hasValue()) {
      ConstraintVariable &CV = CVOpt.getValue();
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(&CV))
        PVC->constrainToWild(CS, Rsn, &PL);
    }
  }
}

// For each pointer type in the declaration of D, add a variable to the
// constraint system for that pointer type.
void ProgramInfo::addVariable(clang::DeclaratorDecl *D,
                              clang::ASTContext *AstContext) {
  assert(!persisted);

  PersistentSourceLoc PLoc = PersistentSourceLoc::mkPSL(D, *AstContext);
  assert(PLoc.valid());

  // We only add a PVConstraint if Variables[PLoc] does not exist.
  // Functions are exempt from this check because they need to be added to the
  // Extern/Static function map even if they are inside a macro expansion.
  if (Variables.find(PLoc) != Variables.end() && !isa<FunctionDecl>(D)) {
    // Two variables can have the same source locations when they are
    // declared inside the same macro expansion. The first instance of the
    // source location will have been constrained to WILD, so it's safe to bail
    // without doing anymore work.
    if (!Rewriter::isRewritable(D->getLocation())) {
      // If we're not in a macro, we should make the constraint variable WILD
      // anyways. This happens if the name of the variable is a macro defined
      // differently is different parts of the program.
      std::string Rsn = "Duplicate source location. Possibly part of a macro.";
      Variables[PLoc]->constrainToWild(CS, Rsn);
    }
    return;
  }

  ConstraintVariable *NewCV = nullptr;

  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    // Function Decls have FVConstraints.
    FVConstraint *F = new FVConstraint(D, *this, *AstContext);
    F->setValidDecl();

    // Handling of PSL collision for functions is different since we need to
    // consider the static and extern function maps.
    if (Variables.find(PLoc) != Variables.end()) {
      // Try to find a previous definition based on function name
      if (!getFuncConstraint(FD, AstContext)) {
        constrainWildIfMacro(F, FD->getLocation());
        insertNewFVConstraint(FD, F, AstContext);
      } else {
        // FIXME: Visiting same function in same source location twice.
        //        This shouldn't happen, but it does for some std lib functions
        //        on our benchmarks programs (vsftpd, lua, etc.). Bailing for
        //        now, but a real fix will catch and prevent this earlier.
      }
      return;
    }

    /* Store the FVConstraint in the global and Variables maps */
    insertNewFVConstraint(FD, F, AstContext);

    NewCV = F;
    // Add mappings from the parameters PLoc to the constraint variables for
    // the parameters.
    for (unsigned i = 0; i < FD->getNumParams(); i++) {
      ParmVarDecl *PVD = FD->getParamDecl(i);
      ConstraintVariable *PV = F->getParamVar(i);
      PV->setValidDecl();
      PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(PVD, *AstContext);
      // Constraint variable is stored on the parent function, so we need to
      // constrain to WILD even if we don't end up storing this in the map.
      constrainWildIfMacro(PV, PVD->getLocation());
      specialCaseVarIntros(PVD, AstContext);
      // It is possible to have a parameter delc in a macro when function is not
      if (Variables.find(PSL) != Variables.end())
        continue;
      Variables[PSL] = PV;
    }

  } else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    assert(!isa<ParmVarDecl>(VD));
    const Type *Ty = VD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    if (Ty->isPointerType() || Ty->isArrayType()) {
      PVConstraint *P = new PVConstraint(D, *this, *AstContext);
      P->setValidDecl();
      NewCV = P;
      std::string VarName = VD->getName();
      if (VD->hasGlobalStorage()) {
          // if we see a definition for this global variable, indicate so in ExternGVars
          if(VD->hasDefinition() || VD->hasDefinition(*AstContext)) {
              ExternGVars[VarName] = true;
          }
          // if we don't, check that we haven't seen one before before setting to false
          else if(!ExternGVars[VarName]) {
              ExternGVars[VarName] = false;
          }
          GlobalVariableSymbols[VarName].insert(P);
      }
      specialCaseVarIntros(D, AstContext);
    }

  } else if (FieldDecl *FlD = dyn_cast<FieldDecl>(D)) {
    const Type *Ty = FlD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    if (Ty->isPointerType() || Ty->isArrayType()) {
      NewCV = new PVConstraint(D, *this, *AstContext);
      NewCV->setValidDecl();
      specialCaseVarIntros(D, AstContext);
    }
  } else
    llvm_unreachable("unknown decl type");

  assert("We shouldn't be adding a null CV to Variables map." && NewCV);
  constrainWildIfMacro(NewCV, D->getLocation());
  Variables[PLoc] = NewCV;
}

bool ProgramInfo::hasPersistentConstraints(Expr *E, ASTContext *C) const {
  auto PSL = PersistentSourceLoc::mkPSL(E, *C);
  // Has constraints only if the PSL is valid.
  return PSL.valid() && ExprConstraintVars.find(PSL) != ExprConstraintVars.end()
      && !ExprConstraintVars.at(PSL).empty();
}

// Get the set of constraint variables for an expression that will persist
// between the constraint generation and rewriting pass. If the expression
// already has a set of persistent constraints, this set is returned. Otherwise,
// the set provided in the arguments is stored persistent and returned. This is
// required for correct cast insertion.
const CVarSet &ProgramInfo::getPersistentConstraints(Expr *E,
                                                     ASTContext *C) const {
  assert (hasPersistentConstraints(E, C) &&
           "Persistent constraints not present.");
  PersistentSourceLoc PLoc = PersistentSourceLoc::mkPSL(E, *C);
  return ExprConstraintVars.at(PLoc);
}

void ProgramInfo::storePersistentConstraints(Expr *E, const CVarSet &Vars,
                                             ASTContext *C) {
  // Store only if the PSL is valid.
  auto PSL = PersistentSourceLoc::mkPSL(E, *C);
  // The check Rewrite::isRewritable is needed here to ensure that the
  // expression is not inside a macro. If the expression is in a macro, then it
  // is possible for there to be multiple expressions that map to the same PSL.
  // This could make it look like the constraint variables for an expression
  // have been computed and cached when the expression has not in fact been
  // visited before. To avoid this, the expression is not cached and instead is
  // recomputed each time it's needed.
  if (PSL.valid() && Rewriter::isRewritable(E->getBeginLoc()))
    ExprConstraintVars[PSL].insert(Vars.begin(), Vars.end());
}

// The Rewriter won't let us re-write things that are in macros. So, we
// should check to see if what we just added was defined within a macro.
// If it was, we should constrain it to top. This is sad. Hopefully,
// someday, the Rewriter will become less lame and let us re-write stuff
// in macros.
void ProgramInfo::constrainWildIfMacro(ConstraintVariable *CV,
                                       SourceLocation Location) {
  std::string Rsn = "Pointer in Macro declaration.";
  if (!Rewriter::isRewritable(Location))
    CV->constrainToWild(CS, Rsn);
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

FVConstraint *ProgramInfo::getFuncConstraint(FunctionDecl *D,
                                             ASTContext *C) const {
  std::string FuncName = D->getNameAsString();
  if (D->isGlobal()) {
    // Is this a global (externally visible) function?
    return getExtFuncDefnConstraint(FuncName);
  } else {
    // Static function.
    auto Psl = PersistentSourceLoc::mkPSL(D, *C);
    std::string FileName = Psl.getFileName();
    return getStaticFuncConstraint(FuncName, FileName);
  }
}

FVConstraint *
    ProgramInfo::getFuncFVConstraint(FunctionDecl *FD, ASTContext *C) {
  std::string FuncName = FD->getNameAsString();
  FVConstraint *FunFVar = nullptr;
  if (FD->isGlobal()) {
    FunFVar = getExtFuncDefnConstraint(FuncName);
    // FIXME: We are being asked to access a function never declared; best action?
    if (FunFVar == nullptr) {
      // make one
      FVConstraint *F = new FVConstraint(FD, *this, *C);
      assert(!F->hasBody());
      assert("FunFVar can only be null if FuncName is not in the map!"
                 && ExternalFunctionFVCons.find(FuncName)
                     == ExternalFunctionFVCons.end());
      ExternalFunctionFVCons[FuncName] = F;
      FunFVar = ExternalFunctionFVCons[FuncName];
    }
  } else {
    auto Psl = PersistentSourceLoc::mkPSL(FD, *C);
    std::string FileName = Psl.getFileName();
    FunFVar = getStaticFuncConstraint(FuncName, FileName);
  }

  return FunFVar;
}

// Given a decl, return the variables for the constraints of the Decl.
// Returns null if a constraint variable could not be found for the decl.
CVarOption ProgramInfo::getVariable(clang::Decl *D, clang::ASTContext *C) {
  assert(!persisted);

  if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
    DeclContext *DC = PD->getParentFunctionOrMethod();
    // This can fail for extern definitions
    if(!DC)
      return CVarOption();
    FunctionDecl *FD = dyn_cast<FunctionDecl>(DC);
    // Get the parameter index with in the function.
    unsigned int PIdx = getParameterIndex(PD, FD);
    // Get corresponding FVConstraint vars.
    FVConstraint *FunFVar = getFuncFVConstraint(FD, C);
    assert(FunFVar != nullptr && "Unable to find function constraints.");
    return CVarOption(*FunFVar->getParamVar(PIdx));

  } else if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    FVConstraint *FunFVar = getFuncFVConstraint(FD, C);
    if (FunFVar == nullptr) {
      llvm::errs() << "No fun constraints for " << FD->getName() << "?!\n";
    }
    return CVarOption(*FunFVar);

  } else /* neither function nor function parameter */ {
    auto I = Variables.find(PersistentSourceLoc::mkPSL(D, *C));
    if (I != Variables.end())
      return CVarOption(*I->second);
    return CVarOption();
  }
}

FVConstraint *
ProgramInfo::getExtFuncDefnConstraint(std::string FuncName) const {
  if (ExternalFunctionFVCons.find(FuncName) != ExternalFunctionFVCons.end()) {
    return ExternalFunctionFVCons.at(FuncName);
  }
  return nullptr;
}

FVConstraint *
ProgramInfo::getStaticFuncConstraint(std::string FuncName,
                                     std::string FileName) const {
  if (StaticFunctionFVCons.find(FileName) != StaticFunctionFVCons.end() &&
      StaticFunctionFVCons.at(FileName).find(FuncName) !=
          StaticFunctionFVCons.at(FileName).end()) {
    return StaticFunctionFVCons.at(FileName).at(FuncName);
  }
  return nullptr;
}

// From the given constraint graph, this method computes the interim constraint
// state that contains constraint vars which are directly assigned WILD and
// other constraint vars that have been determined to be WILD because they
// depend on other constraint vars that are directly assigned WILD.
bool ProgramInfo::computeInterimConstraintState
    (const std::set<std::string> &FilePaths) {

  // Get all the valid vars of interest i.e., all the Vars that are present
  // in one of the files being compiled.
  CAtoms ValidVarsVec;
  std::set<Atom *> AllValidVars;
  for (const auto &I : Variables) {
    std::string FileName = I.first.getFileName();
    ConstraintVariable *C = I.second;
    if (C->isForValidDecl()) {
      CAtoms tmp = getVarsFromConstraint(C);
      AllValidVars.insert(tmp.begin(), tmp.end());
      if (FilePaths.count(FileName) || FileName.find(BaseDir) != std::string::npos) {
        //if (C->isForValidDecl()) {
        ValidVarsVec.insert(ValidVarsVec.begin(), tmp.begin(), tmp.end());
        //}
      }
    }
  }

  /*for (const auto &I : ExprConstraintVars) {
    std::string FileName = I.first.getFileName();
    if (FilePaths.count(FileName) || FileName.find(BaseDir) != std::string::npos) {
      for (auto *C : I.second) {
        //if (C->isForValidDecl()) {
        CAtoms tmp = getVarsFromConstraint(C);
        ValidVarsVec.insert(ValidVarsVec.begin(), tmp.begin(), tmp.end());
        //}
      }
    }
  }*/
  // Make that into set, for efficiency.
  std::set<Atom *> ValidVarsS;
  CVars ValidVarsKey;
  CVars AllValidVarsKey;
  ValidVarsS.insert(ValidVarsVec.begin(), ValidVarsVec.end());

  std::transform(ValidVarsS.begin() , ValidVarsS.end(),
                 std::inserter(ValidVarsKey, ValidVarsKey.end()) ,
                 [](const Atom *val) {
    if (const VarAtom *VA = dyn_cast<VarAtom>(val)) {
      return VA->getLoc();
    }
    return (uint32_t)0;
  });

  std::transform(AllValidVars.begin() , AllValidVars.end(),
                 std::inserter(AllValidVarsKey, AllValidVarsKey.end()) ,
                 [](const Atom *val) {
                   if (const VarAtom *VA = dyn_cast<VarAtom>(val)) {
                     return VA->getLoc();
                   }
                   return (uint32_t)0;
  });

  CState.Clear();

  auto &RCMap = CState.RCMap;
  auto &SrcWMap = CState.SrcWMap;
  auto &TotalNDirectWPtrs = CState.TotalNonDirectWildPointers;
  auto &InSrInDirectWPtrs = CState.InSrcNonDirectWildPointers;
  CVars &WildPtrs = CState.AllWildPtrs;
  CVars &InSrcW = CState.InSrcWildPtrs;
  WildPtrs.clear();
  std::set<Atom *> DirectWildVarAtoms;
  std::set<Atom *> IndirectWildAtoms;
  auto &ChkCG = CS.getChkCG();
  ChkCG.getSuccessors(CS.getWild(), DirectWildVarAtoms);

  CVars TmpCGrp;
  for (auto *A : DirectWildVarAtoms) {
    auto *VA = dyn_cast<VarAtom>(A);
    if (VA == nullptr)
      continue;

    TmpCGrp.clear();
    ChkCG.visitBreadthFirst(VA,
      [VA, &DirectWildVarAtoms, &AllValidVars, &RCMap, &TmpCGrp](Atom *SearchAtom) {
        auto *SearchVA = dyn_cast<VarAtom>(SearchAtom);
        if (SearchVA != nullptr &&
              DirectWildVarAtoms.find(SearchVA) == DirectWildVarAtoms.end() &&
              AllValidVars.find(SearchVA) != AllValidVars.end()) {
          RCMap[SearchVA->getLoc()].insert(VA->getLoc());
          TmpCGrp.insert(SearchVA->getLoc());
        }
      });

    TotalNDirectWPtrs.insert(TmpCGrp.begin(), TmpCGrp.end());
    // Should we consider only pointers which with in the source files or
    // external pointers that affected pointers within the source files.
    //if (!TmpCGrp.empty() || ValidVarsS.find(VA) != ValidVarsS.end()) {
    WildPtrs.insert(VA->getLoc());
    CVars &CGrp = SrcWMap[VA->getLoc()];
    CGrp.insert(TmpCGrp.begin(), TmpCGrp.end());
    //}
  }
  //auto *AA = CS.getVar(11513);
  //AA->dump();
  findIntersection(WildPtrs, ValidVarsKey, InSrcW);
  findIntersection(TotalNDirectWPtrs, ValidVarsKey, InSrInDirectWPtrs);

  auto &WildPtrsReason = CState.RealWildPtrsWithReasons;

  for (auto currC : CS.getConstraints()) {
    if (Geq *EC = dyn_cast<Geq>(currC)) {
      VarAtom *VLhs = dyn_cast<VarAtom>(EC->getLHS());
      if (EC->constraintIsChecked() && dyn_cast<WildAtom>(EC->getRHS())) {
        WildPtrsReason[VLhs->getLoc()].WildPtrReason = EC->getReason();
        if (!EC->FileName.empty() && EC->LineNo != 0) {
          WildPtrsReason[VLhs->getLoc()].IsValid = true;
          WildPtrsReason[VLhs->getLoc()].SourceFileName = EC->FileName;
          WildPtrsReason[VLhs->getLoc()].LineNo = EC->LineNo;
          WildPtrsReason[VLhs->getLoc()].ColStartS = EC->ColStart;
          WildPtrsReason[VLhs->getLoc()].ColStartE = EC->ColEnd;
        }
      }
    }
  }

  for ( const auto &I : Variables ) {
    PersistentSourceLoc L = I.first;
    std::string FilePath = L.getFileName();
    if (canWrite(FilePath)) {
      CState.ValidSourceFiles.insert(FilePath);
    } else {
      continue;
    }
    ConstraintVariable *CV = I.second;
    if (PVConstraint *PV = dyn_cast<PVConstraint>(CV)) {
      for (auto ck : PV->getCvars()) {
        if (VarAtom *VA = dyn_cast<VarAtom>(ck)) {
          CState.PtrSourceMap[VA->getLoc()] =
              const_cast<PersistentSourceLoc *>(&(I.first));
        }
      }
    }
    if (FVConstraint *FV = dyn_cast<FVConstraint>(CV)) {
      if (FV->getReturnVar()) {
        if (PVConstraint *RPV = dyn_cast<PVConstraint>(FV->getReturnVar())) {
          for (auto ck : RPV->getCvars()) {
            if (VarAtom *VA = dyn_cast<VarAtom>(ck)) {
              CState.PtrSourceMap[VA->getLoc()] =
                  const_cast<PersistentSourceLoc *>(&(I.first));
            }
          }
        }
      }
    }
  }

  return true;
}

void ProgramInfo::setTypeParamBinding(CallExpr *CE, unsigned int TypeVarIdx,
                                      ConstraintVariable *CV, ASTContext *C) {

  auto PSL = PersistentSourceLoc::mkPSL(CE, *C);
  auto CallMap = TypeParamBindings[PSL];
  assert("Attempting to overwrite type param binding in ProgramInfo."
             && CallMap.find(TypeVarIdx) == CallMap.end());

  TypeParamBindings[PSL][TypeVarIdx] = CV;
}

bool ProgramInfo::hasTypeParamBindings(CallExpr *CE, ASTContext *C) const {
  auto PSL = PersistentSourceLoc::mkPSL(CE, *C);
  return TypeParamBindings.find(PSL) != TypeParamBindings.end();
}

const ProgramInfo::CallTypeParamBindingsT &
    ProgramInfo::getTypeParamBindings(CallExpr *CE, ASTContext *C) const {
  auto PSL = PersistentSourceLoc::mkPSL(CE, *C);
  assert("Type parameter bindings could not be found."
             && TypeParamBindings.find(PSL) != TypeParamBindings.end());
  return TypeParamBindings.at(PSL);
}