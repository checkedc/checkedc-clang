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
#include "clang/CConv/ConstraintBuilder.h"
#include "clang/CConv/MappingVisitor.h"
#include <sstream>

using namespace clang;

ProgramInfo::ProgramInfo() :
  freeKey(0), persisted(true) {
  ArrBoundsInfo = new ArrayBoundsInformation(*this);
  ExternalFunctionDeclFVCons.clear();
  ExternalFunctionDefnFVCons.clear();
  StaticFunctionDeclFVCons.clear();
  StaticFunctionDefnFVCons.clear();
  MultipleRewrites = false;
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
    O << "Func Name:" << DefM.first << " => ";
    for (const auto &Tmp : DefM.second) {
      O << " File Name:"<< Tmp.first << " => \n";
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
    const std::set<ConstraintVariable *> &S = I.second;
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
  dumpExtFuncMap(ExternalFunctionDefnFVCons, O);
  O << "External Function Declarations\n";
  dumpExtFuncMap(ExternalFunctionDeclFVCons, O);
  O << "Static Function Definitions\n";
  dumpStaticFuncMap(StaticFunctionDefnFVCons, O);
  O << "Static Function Declarations\n";
  dumpStaticFuncMap(StaticFunctionDeclFVCons, O);
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
    const std::set<ConstraintVariable *> &S = I.second;

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
  dumpExtFuncMapJson(ExternalFunctionDefnFVCons, O);
  O << "], \"ExternalFunctionDeclarations\":[";
  dumpExtFuncMapJson(ExternalFunctionDeclFVCons, O);
  O << "], \"StaticFunctionDefinitions\":[";
  dumpStaticFuncMapJson(StaticFunctionDefnFVCons, O);
  O << "], \"StaticFunctionDeclarations\":[";
  dumpStaticFuncMapJson(StaticFunctionDeclFVCons, O);
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
    O << "Merge multiple function declaration:" << !SeperateMultipleFuncDecls << "\n";
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

}

// Check the equality of VTy and UTy. There are some specific rules that
// fire, and a general check is yet to be implemented. 
bool ProgramInfo::checkStructuralEquality(std::set<ConstraintVariable *> V,
                                          std::set<ConstraintVariable *> U,
                                          QualType VTy,
                                          QualType UTy) 
{
  // First specific rule: Are these types directly equal? 
  if (VTy == UTy) {
    return true;
  } else {
    // Further structural checking is TODO.
    return false;
  } 
}

bool ProgramInfo::checkStructuralEquality(QualType D, QualType S) {
  if (D == S)
    return true;

  return D->isPointerType() == S->isPointerType();
}

bool ProgramInfo::isExplicitCastSafe(clang::QualType DstType,
                                     clang::QualType SrcType) {

  // Check if both types are same.
  if (SrcType == DstType)
    return true;

  const clang::Type *SrcTypePtr = SrcType.getTypePtr();
  const clang::Type *DstTypePtr = DstType.getTypePtr();

  const clang::PointerType *SrcPtrTypePtr = dyn_cast<PointerType>(SrcTypePtr);
  const clang::PointerType *DstPtrTypePtr = dyn_cast<PointerType>(DstTypePtr);

  // Both are pointers? check their pointee
  if (SrcPtrTypePtr && DstPtrTypePtr)
    return isExplicitCastSafe(DstPtrTypePtr->getPointeeType(),
                              SrcPtrTypePtr->getPointeeType());
  // Only one of them is pointer?
  if (SrcPtrTypePtr || DstPtrTypePtr)
    return false;

  // If both are not scalar types? Then the types must be exactly same.
  if (!(SrcTypePtr->isScalarType() && DstTypePtr->isScalarType()))
    return SrcTypePtr == DstTypePtr;

  // Check if both types are compatible.
  bool BothNotChar = SrcTypePtr->isCharType() ^ DstTypePtr->isCharType();
  bool BothNotInt =
      SrcTypePtr->isIntegerType() ^ DstTypePtr->isIntegerType();
  bool BothNotFloat =
      SrcTypePtr->isFloatingType() ^ DstTypePtr->isFloatingType();


  return !(BothNotChar || BothNotInt || BothNotFloat);
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

  // Multiple Variables can be at the same PersistentSourceLoc. We should
  // constrain that everything that is at the same location is explicitly
  // equal.
  for (const auto &V : Variables) {
    std::set<ConstraintVariable *> C = V.second;

    if (C.size() > 1) {
      std::set<ConstraintVariable *>::iterator I = C.begin();
      std::set<ConstraintVariable *>::iterator J = C.begin();
      ++J;

      while (J != C.end()) {
        constrainConsVarGeq(*I, *J, CS, nullptr);
        ++I;
        ++J;
      }
    }
  }

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
        constrainConsVarGeq(*I, *J, CS, nullptr);
        ++I;
        ++J;
      }
    }
  }

  if (!SeperateMultipleFuncDecls) {
    int Gap = 0;
    for (auto &S : ExternalFunctionDeclFVCons) {
      std::string Fname = S.first;
      std::set<FVConstraint *> &P = S.second;

      if (P.size() > 1) {
        std::set<FVConstraint *>::iterator I = P.begin();
        std::set<FVConstraint *>::iterator J = P.begin();
        ++J;

        while (J != P.end()) {
          FVConstraint *P1 = *I;
          FVConstraint *P2 = *J;

          if (P2->hasBody()) { // skip over decl with fun body
            Gap = 1;
            ++J;
            continue;
          }
          // Constrain the return values to be equal.
          if (!P1->hasBody() && !P2->hasBody()) {
            constrainConsVarGeq(P1->getReturnVars(), P2->getReturnVars(), CS,
                                nullptr);

            // Constrain the parameters to be equal, if the parameter arity is
            // the same. If it is not the same, constrain both to be wild.
            if (P1->numParams() == P2->numParams()) {
              for (unsigned i = 0; i < P1->numParams(); i++) {
                constrainConsVarGeq(P1->getParamVar(i), P2->getParamVar(i), CS,
                                    nullptr);
              }

            } else {
              // It could be the case that P1 or P2 is missing a prototype, in
              // which case we don't need to constrain anything.
              if (P1->hasProtoType() && P2->hasProtoType()) {
                // Nope, we have no choice. Constrain everything to wild.
                std::string rsn = "Return value of function:" + P1->getName();
                P1->constrainToWild(CS, rsn, true);
                P2->constrainToWild(CS, rsn, true);
              }
            }
          }
          ++I;
          if (!Gap) {
            ++J;
          } else {
            Gap = 0;
          }
        }
      }
    }
  }


  // For every global function that is an unresolved external, constrain 
  // its parameter types to be wild. Unless it has a bounds-safe annotation. 
  for (const auto &U : ExternFunctions) {
    // If we've seen this symbol, but never seen a body for it, constrain
    // everything about it.
    if (U.second == false && isExternOkay(U.first) == false) {
      // Some global symbols we don't need to constrain to wild, like 
      // malloc and free. Check those here and skip if we find them. 
      std::string FuncName = U.first;
      auto FuncDeclFVIterator =
          ExternalFunctionDeclFVCons.find(FuncName);
      assert(FuncDeclFVIterator != ExternalFunctionDeclFVCons.end());
      const std::set<FVConstraint *> &Gs = (*FuncDeclFVIterator).second;

      for (const auto GIterator : Gs) {
        auto G = GIterator;
        for (const auto &U : G->getReturnVars()) {
          std::string Rsn = "Return value of an external function:" + FuncName;
          U->constrainToWild(CS, Rsn, true);
        }
        std::string rsn = "Inner pointer of a parameter to external function.";
        for (unsigned i = 0; i < G->numParams(); i++)
          for (const auto &PVar : G->getParamVar(i))
            PVar->constrainToWild(CS, rsn, true);
      }
    }
  }

  return true;
}

bool ProgramInfo::isAnExternFunction(const std::string &FName) {
  return !ExternFunctions[FName];
}

void ProgramInfo::seeFunctionDecl(FunctionDecl *F, ASTContext *C) {
  if (!F->isGlobal())
    return;

  // Track if we've seen a body for this function or not.
  std::string Fname = F->getNameAsString();
  if (!ExternFunctions[Fname])
    ExternFunctions[Fname] = (F->isThisDeclarationADefinition() && F->hasBody());

  // Look up the constraint variables for the return type and parameter 
  // declarations of this function, if any.
  /*
  std::set<uint32_t> returnVars;
  std::vector<std::set<uint32_t> > parameterVars(F->getNumParams());
  PersistentSourceLoc PLoc = PersistentSourceLoc::mkPSL(F, *C);
  int i = 0;

  std::set<ConstraintVariable*> FV = getVariable(F, C);
  assert(FV.size() == 1);
  const ConstraintVariable *PFV = (*(FV.begin()));
  assert(PFV != nullptr);
  const FVConstraint *FVC = dyn_cast<FVConstraint>(PFV);
  assert(FVC != nullptr);

  //returnVars = FVC->getReturnVars();
  //unsigned i = 0;
  //for (unsigned i = 0; i < FVC->numParams(); i++) {
  //  parameterVars.push_back(FVC->getParamVar(i));
  //}

  assert(PLoc.valid());
  GlobalFunctionSymbol *GF = 
    new GlobalFunctionSymbol(fn, PLoc, parameterVars, returnVars);

  // Add this to the map of global symbols. 
  std::map<std::string, std::set<GlobalSymbol*> >::iterator it = 
    GlobalFunctionSymbols.find(fn);
  
  if (it == GlobalFunctionSymbols.end()) {
    std::set<GlobalSymbol*> N;
    N.insert(GF);
    GlobalFunctionSymbols.insert(std::pair<std::string, std::set<GlobalSymbol*> >
      (fn, N));
  } else {
    (*it).second.insert(GF);
  }*/
}

void ProgramInfo::seeGlobalDecl(clang::VarDecl *G, ASTContext *C) {
  std::string VarName = G->getName();

  // Add this to the map of global symbols.
  std::set<PVConstraint *> ToAdd;
  // Get the constraint variable directly.
  std::set<ConstraintVariable *> K;
  VariableMap::iterator I = Variables.find(PersistentSourceLoc::mkPSL(G, *C));
  if (I != Variables.end()) {
    K = I->second;
  }
  for (const auto &J : K)
    if (PVConstraint *FJ = dyn_cast<PVConstraint>(J))
      ToAdd.insert(FJ);

  assert(ToAdd.size() > 0);

  if (GlobalVariableSymbols.find(VarName) != GlobalVariableSymbols.end()) {
    GlobalVariableSymbols[VarName].insert(ToAdd.begin(), ToAdd.end());
  } else {
    GlobalVariableSymbols[VarName] = ToAdd;
  }

}

// Populate Variables, VarDeclToStatement, RVariables, and DepthMap with
// AST data structures that correspond do the data stored in PDMap and
// ReversePDMap.
void ProgramInfo::enterCompilationUnit(ASTContext &Context) {
  assert(persisted == true);
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
  assert(persisted == false);
  persisted = true;
  return;
}

template <typename T>
bool ProgramInfo::hasConstraintType(std::set<ConstraintVariable *> &S) {
  for (const auto &I : S) {
    if (isa<T>(I)) {
      return true;
    }
  }
  return false;
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
    MultipleRewrites = true;
  }
  return RetVal;
}

bool
ProgramInfo::insertIntoStaticFunctionMap(StaticFunctionMapType &Map,
                                         const std::string &FuncName,
                                         const std::string &FileName,
                                         std::set<FVConstraint *> &ToIns) {
  bool RetVal = false;
  if (Map.find(FuncName) == Map.end()) {
    Map[FuncName][FileName] = ToIns;
    RetVal = true;
  } else if (Map[FuncName].find(FileName) == Map[FuncName].end()) {
    Map[FuncName][FileName] = ToIns;
    RetVal = true;
  } else {
    MultipleRewrites = true;
  }
  return RetVal;
}

void
ProgramInfo::insertNewFVConstraints(FunctionDecl *FD,
                                   std::set<FVConstraint *> &FVcons,
                                   ASTContext *C) {
  std::string FuncName = FD->getNameAsString();
  if (FD->isGlobal()) {
    // external method.
    if (FD->isThisDeclarationADefinition() && FD->hasBody()) {
      // Function definition.
      insertIntoExternalFunctionMap(ExternalFunctionDefnFVCons,
                                    FuncName, FVcons);
    } else {
      insertIntoExternalFunctionMap(ExternalFunctionDeclFVCons,
                                    FuncName, FVcons);
    }
  } else {
    // static method
    auto Psl = PersistentSourceLoc::mkPSL(FD, *C);
    std::string FuncFileName = Psl.getFileName();
    if (FD->isThisDeclarationADefinition() && FD->hasBody()) {
      // Function definition.
      insertIntoStaticFunctionMap(StaticFunctionDefnFVCons, FuncName,
                                  FuncFileName, FVcons);
    } else {
      insertIntoStaticFunctionMap(StaticFunctionDeclFVCons, FuncName,
                                  FuncFileName, FVcons);
    }
  }
}

// For each pointer type in the declaration of D, add a variable to the
// constraint system for that pointer type.
bool ProgramInfo::addVariable(DeclaratorDecl *D, ASTContext *astContext) {
  assert(persisted == false);

  PersistentSourceLoc PLoc = PersistentSourceLoc::mkPSL(D, *astContext);
  assert(PLoc.valid());

  // We only add a PVConstraint or an FVConstraint if the set at
  // Variables[PLoc] does not contain one already. TODO: Explain why would this happen
  std::set<ConstraintVariable *> &S = Variables[PLoc];

  // Function Decls have FVConstraints. Function pointers have PVConstraints;
  // see below
  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    const Type *Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    assert(Ty->isFunctionType());

    // Create a function value for the type.
    // process the function constraint only if it doesn't exist
    if (!hasConstraintType<FVConstraint>(S)) {
      FVConstraint *F = new FVConstraint(D, freeKey, CS, *astContext);
      S.insert(F);

      // If this is a function. Save the created constraint.
      // this needed for resolving function subtypes later.
      // we create a unique key for the declaration and definition
      // of a function.
      // We save the mapping between these unique keys.
      // This is needed so that later when we have to
      // resolve function subtyping. where for each function
      // we need access to teh definition and declaration
      // constraint variables.
      std::string FuncName = FD->getNameAsString();
      // FV Constraints to insert.
      std::set<FVConstraint *> NewFVars;
      NewFVars.insert(F);
      insertNewFVConstraints(FD, NewFVars, astContext);

      // Add mappings from the parameters PLoc to the constraint variables for
      // the parameters.
      // We just created this, so they should be equal.
      assert(FD->getNumParams() == F->numParams());
      for (unsigned i = 0; i < FD->getNumParams(); i++) {
        ParmVarDecl *PVD = FD->getParamDecl(i);
        std::set<ConstraintVariable *> S = F->getParamVar(i);
        if (S.size()) {
          PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(PVD, *astContext);
          Variables[PSL].insert(S.begin(), S.end());
        }
      }
    }
  } else {
    const Type *Ty = nullptr;
    if (VarDecl *VD = dyn_cast<VarDecl>(D))
      Ty = VD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    else if (FieldDecl *FD = dyn_cast<FieldDecl>(D))
      Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    else
      llvm_unreachable("unknown decl type");

    // We will add a PVConstraint even for FunPtrs
    if (Ty->isPointerType() || Ty->isArrayType()) {
      // Create a pointer value for the type.
      if (!hasConstraintType<PVConstraint>(S)) {
        PVConstraint *P = new PVConstraint(D, freeKey, CS, *astContext);
        S.insert(P);
      }
    }
  }

  // The Rewriter won't let us re-write things that are in macros. So, we 
  // should check to see if what we just added was defined within a macro.
  // If it was, we should constrain it to top. This is sad. Hopefully, 
  // someday, the Rewriter will become less lame and let us re-write stuff
  // in macros.
  std::string Rsn = "Pointer in Macro declaration.";
  if (!Rewriter::isRewritable(D->getLocation())) 
    for (const auto &C : S)
      C->constrainToWild(CS, Rsn, false);

  return true;
}

// This is a bit of a hack. What we need to do is traverse the AST in a
// bottom-up manner, and, for a given expression, decide which singular,
// if any, constraint variable is involved in that expression. However,
// in the current version of clang (3.8.1), bottom-up traversal is not
// supported. So instead, we do a manual top-down traversal, considering
// the different cases and their meaning on the value of the constraint
// variable involved. This is probably incomplete, but, we're going to
// go with it for now.
//
// V is (currentVariable, baseVariable, limitVariable)
// E is an expression to recursively traverse.
//
// Returns true if E resolves to a constraint variable q_i and the
// currentVariable field of V is that constraint variable. Returns false if
// a constraint variable cannot be found.
// ifc mirrors the inFunctionContext boolean parameter to getVariable. 
std::set<ConstraintVariable *> 
ProgramInfo::getVariableHelper( Expr                            *E,
                                std::set<ConstraintVariable *>  V,
                                ASTContext                      *C,
                                bool Ifc)
{
  E = E->IgnoreParenImpCasts();
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
    return getVariable(DRE->getDecl(), C, Ifc);
  } else if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
    return getVariable(ME->getMemberDecl(), C, Ifc);
  } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
    std::set<ConstraintVariable *> T1 = getVariableHelper(BO->getLHS(), V, C, Ifc);
    std::set<ConstraintVariable *> T2 = getVariableHelper(BO->getRHS(), V, C, Ifc);
    T1.insert(T2.begin(), T2.end());
    return T1;
  } else if (ArraySubscriptExpr *AE = dyn_cast<ArraySubscriptExpr>(E)) {
    // In an array subscript, we want to do something sort of similar to taking
    // the address or doing a dereference. 
    std::set<ConstraintVariable *> T = getVariableHelper(AE->getBase(), V, C, Ifc);
    std::set<ConstraintVariable *> tmp;
    for (const auto &CV : T) {
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
        // Subtract one from this constraint. If that generates an empty 
        // constraint, then, don't add it 
        CAtoms C = PVC->getCvars();
        if (C.size() > 0) {
          C.erase(C.begin());
          if (C.size() > 0) {
            bool a = PVC->getArrPresent();
            bool c = PVC->getItypePresent();
            std::string d = PVC->getItype();
            FVConstraint *b = PVC->getFV();
            tmp.insert(new PVConstraint(C, PVC->getTy(), PVC->getName(),
                                        b, a, c, d));
          }
        }
      }
    }

    T.swap(tmp);
    return T;
  } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
    std::set<ConstraintVariable *> T = 
      getVariableHelper(UO->getSubExpr(), V, C, Ifc);
   
    std::set<ConstraintVariable *> tmp;
    if (UO->getOpcode() == UO_Deref || UO->getOpcode() == UO_AddrOf) {
      for (const auto &CV : T) {
        if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
          CAtoms C = PVC->getCvars();
          if (UO->getOpcode() == UO_Deref) {
            // Subtract one from this constraint. If that generates an empty
            // constraint, then, don't add it
            if (C.size() > 0) {
              C.erase(C.begin());
              if (C.size() > 0) {
                bool a = PVC->getArrPresent();
                FVConstraint *b = PVC->getFV();
                bool c = PVC->getItypePresent();
                std::string d = PVC->getItype();
                tmp.insert(new PVConstraint(C, PVC->getTy(), PVC->getName(),
                                            b, a, c, d));
              }
            }
          } else { // AddrOf
              C.insert(C.begin(), CS.getPtr());
              // FIXME: revisit the following -- probably wrong.
              bool a = PVC->getArrPresent();
              FVConstraint *b = PVC->getFV();
              bool c = PVC->getItypePresent();
              std::string d = PVC->getItype();
              tmp.insert(new PVConstraint(C, PVC->getTy(), PVC->getName(),
                                          b, a, c, d));
          }
        } else if (!(UO->getOpcode() == UO_AddrOf)) { // no-op for FPs
          llvm_unreachable("Shouldn't dereference a function pointer!");
        }
      }
      T.swap(tmp);
    }

    return T;
  } else if (ImplicitCastExpr *IE = dyn_cast<ImplicitCastExpr>(E)) {
    return getVariableHelper(IE->getSubExpr(), V, C, Ifc);
  } else if (ExplicitCastExpr *ECE = dyn_cast<ExplicitCastExpr>(E)) {
    return getVariableHelper(ECE->getSubExpr(), V, C, Ifc);
  } else if (ParenExpr *PE = dyn_cast<ParenExpr>(E)) {
    return getVariableHelper(PE->getSubExpr(), V, C, Ifc);
  } else if (CHKCBindTemporaryExpr *CBE = dyn_cast<CHKCBindTemporaryExpr>(E)) {
    return getVariableHelper(CBE->getSubExpr(), V, C, Ifc);
  } else if (CallExpr *CE = dyn_cast<CallExpr>(E)) {
    // Call expression should always get out-of context
    // constraint variable.
    Ifc = false;
    // Here, we need to look up the target of the call and return the
    // constraints for the return value of that function.
    Decl *D = CE->getCalleeDecl();
    if (D == nullptr) {
      // There are a few reasons that we couldn't get a decl. For example,
      // the call could be done through an array subscript. 
      Expr *CalledExpr = CE->getCallee();
      std::set<ConstraintVariable *> tmp = getVariableHelper(CalledExpr,
                                                             V, C, Ifc);
      std::set<ConstraintVariable *> T;

      for (ConstraintVariable *C : tmp) {
        if (FVConstraint *FV = dyn_cast<FVConstraint>(C)) {
          T.insert(FV->getReturnVars().begin(), FV->getReturnVars().end());
        } else if (PVConstraint *PV = dyn_cast<PVConstraint>(C)) {
          if (FVConstraint *FV = PV->getFV()) {
            T.insert(FV->getReturnVars().begin(), FV->getReturnVars().end());
          }
        }
      }

      return T;
    }
    assert(D != nullptr);
    // D could be a FunctionDecl, or a VarDecl, or a FieldDecl. 
    // Really it could be any DeclaratorDecl. 
    if (DeclaratorDecl *FD = dyn_cast<DeclaratorDecl>(D)) {
      std::set<ConstraintVariable *> CS = getVariable(FD, C, Ifc);
      std::set<ConstraintVariable *> TR;
      FVConstraint *FVC = nullptr;
      for (const auto &J : CS) {
        if (FVConstraint *tmp = dyn_cast<FVConstraint>(J))
          // The constraint we retrieved is a function constraint already.
          // This happens if what is being called is a reference to a 
          // function declaration, but it isn't all that can happen.
          FVC = tmp;
        else if (PVConstraint *tmp = dyn_cast<PVConstraint>(J))
          if (FVConstraint *tmp2 = tmp->getFV())
            // Or, we could have a PVConstraint to a function pointer. 
            // In that case, the function pointer value will work just
            // as well.
            FVC = tmp2;
      }

      if (FVC) {
        TR.insert(FVC->getReturnVars().begin(), FVC->getReturnVars().end());
      } else {
        // Our options are slim. For some reason, we have failed to find a 
        // FVConstraint for the Decl that we are calling. This can't be good
        // so we should constrain everything in the caller to top. We can
        // fake this by returning a nullary-ish FVConstraint and that will
        // make the logic above us freak out and over-constrain everything.
        TR.insert(new FVConstraint()); 
      }

      return TR;
    } else {
      // If it ISN'T, though... what to do? How could this happen?
      llvm_unreachable("TODO");
    }
  } else if (ConditionalOperator *CO = dyn_cast<ConditionalOperator>(E)) {
    std::set<ConstraintVariable *> T;
    std::set<ConstraintVariable *> R;
    // The condition is not what's returned by the expression, so do not include its var
    //T = getVariableHelper(CO->getCond(), V, C, Ifc);
    //R.insert(T.begin(), T.end());
    T = getVariableHelper(CO->getLHS(), V, C, Ifc);
    R.insert(T.begin(), T.end());
    T = getVariableHelper(CO->getRHS(), V, C, Ifc);
    R.insert(T.begin(), T.end());
    return R;
  } else if (StringLiteral *exr = dyn_cast<StringLiteral>(E)) {
    // If this is a string literal. i.e., "foo".
    // We create a new constraint variable and constraint it to an Nt_array.
    std::set<ConstraintVariable *> T;
    // Create a new constraint var number and make it NTArr.
    CAtoms V;
    V.push_back(CS.getNTArr());
    ConstraintVariable *newC = new PointerVariableConstraint(V,
                                                             "const char*",
                                                             exr->getBytes(),
                                                             nullptr,
                                                             false,
                                                             false, "");
    T.insert(newC);
    return T;

  } else {
    return std::set<ConstraintVariable *>();
  }
}


std::string ProgramInfo::getUniqueDeclKey(Decl *D, ASTContext *C) {
  auto Psl = PersistentSourceLoc::mkPSL(D, *C);
  std::string FileName = Psl.getFileName() + ":" +
                         std::to_string(Psl.getLineNo());
  std::string Dname = D->getDeclKindName();
  if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
    Dname = FD->getNameAsString();
  }
  std::string DeclKey = FileName + ":" + Dname;
  return DeclKey;
}

std::string ProgramInfo::getUniqueFuncKey(FunctionDecl *D,
                                          ASTContext *C) {
  // Get unique key for a function: which is function name,
  // file and line number.
  if (FunctionDecl *FuncDef = getDefinition(D)) {
    D = FuncDef;
  }
  return getUniqueDeclKey(D, C);
}

std::set<FVConstraint *>&
ProgramInfo::getOnDemandFuncDeclarationConstraint(FunctionDecl *D,
                                                  ASTContext *C) {

  std::string FuncName = D->getNameAsString();
  if (D->isGlobal()) {
    // Is this an external function?
    if (ExternalFunctionDeclFVCons.find(FuncName) ==
        ExternalFunctionDeclFVCons.end()) {
      // Create an on demand FVConstraint.
      FVConstraint *F = new FVConstraint(D, freeKey, CS, *C);
      // Set has body is false, as this is for function declaration.
      F->setHasBody(false);
      ExternalFunctionDeclFVCons[FuncName].insert(F);
    }

    return ExternalFunctionDeclFVCons[FuncName];
  } else {
    // Static function.
    auto Psl = PersistentSourceLoc::mkPSL(D, *C);
    std::string FileName = Psl.getFileName();
    if (StaticFunctionDeclFVCons.find(FuncName) ==
        StaticFunctionDeclFVCons.end() ||
        StaticFunctionDeclFVCons[FuncName].find(FileName) ==
        StaticFunctionDeclFVCons[FuncName].end()) {
      FVConstraint *F = new FVConstraint(D, freeKey, CS, *C);
      // Set has body is false, as this is for function declaration.
      F->setHasBody(false);
      StaticFunctionDeclFVCons[FuncName][FileName].insert(F);
    }

    return StaticFunctionDeclFVCons[FuncName][FileName];

  }
}

std::set<FVConstraint *> *
ProgramInfo::getFuncDeclConstraints(FunctionDecl *D, ASTContext *C) {
  std::string FuncName = D->getNameAsString();
  if (D->isGlobal()) {
    return getExtFuncDeclConstraintSet(FuncName);
  } else {
    // Static function.
    auto Psl = PersistentSourceLoc::mkPSL(D, *C);
    return getStaticFuncDeclConstraintSet(FuncName, Psl.getFileName());
  }

}

std::set<FVConstraint *> *
ProgramInfo::getFuncDefnConstraints(FunctionDecl *D, ASTContext *C) {

  std::string FuncName = D->getNameAsString();
  if (D->isGlobal()) {
    // Is this an external function?
    if (ExternalFunctionDefnFVCons.find(FuncName) !=
           ExternalFunctionDefnFVCons.end()) {
      return &ExternalFunctionDefnFVCons[FuncName];
    }
  } else {
    // Static function.
    auto Psl = PersistentSourceLoc::mkPSL(D, *C);
    std::string FileName = Psl.getFileName();
    if (StaticFunctionDefnFVCons.find(FuncName) !=
           StaticFunctionDefnFVCons.end() &&
        StaticFunctionDefnFVCons[FuncName].find(FileName) !=
           StaticFunctionDefnFVCons[FuncName].end()) {
      return &StaticFunctionDefnFVCons[FuncName][FileName];
    }
  }
  return nullptr;
}

std::set<ConstraintVariable *>
ProgramInfo::getVariable(clang::Decl *D, clang::ASTContext *C, FunctionDecl *FD,
                         int PIdx) {
  // If this is a parameter.
  if (PIdx >= 0) {
    // Get the parameter index of the requested function declaration.
    D = FD->getParamDecl(PIdx);
  } else {
    // This is the return value of the function.
    D = FD;
  }
  VariableMap::iterator I =
      Variables.find(PersistentSourceLoc::mkPSL(D, *C));
  assert(I != Variables.end());
  return I->second;

}

std::set<ConstraintVariable *>
ProgramInfo::getVariable(clang::Decl *D, clang::ASTContext *C,
                         bool InFuncCtx) {
  // Here, we auto-correct the inFunctionContext flag.
  // If someone is asking for in context variable of a function
  // always give the declaration context.

  // If this a function declaration set in context to false.
  if (dyn_cast<FunctionDecl>(D)) {
    InFuncCtx = false;
  }
  return getVariableOnDemand(D, C, InFuncCtx);
}

std::set<FVConstraint *> *getFuncFVConstraints(FunctionDecl *FD,
                                               ProgramInfo &I,
                                               ASTContext *C,
                                               bool Defn) {
  std::string FuncName = FD->getNameAsString();
  std::set<FVConstraint *> *FunFVars = nullptr;

  if (Defn) {
    // External function definition.
    if (FD->isGlobal()) {
      FunFVars = I.getExtFuncDefnConstraintSet(FuncName);
    } else {
      auto Psl = PersistentSourceLoc::mkPSL(FD, *C);
      std::string FileName = Psl.getFileName();
      FunFVars = I.getStaticFuncDefnConstraintSet(FuncName, FileName);
    }

  }

  if (FunFVars == nullptr) {
    // Try to get declaration constraints.
    FunFVars = &(I.getOnDemandFuncDeclarationConstraint(FD, C));
  }

  return FunFVars;
}


// Given a decl, return the variables for the constraints of the Decl.
std::set<ConstraintVariable *>
ProgramInfo::getVariableOnDemand(Decl *D, ASTContext *C,
                                 bool InFuncCtx) {
  assert(persisted == false);
  // Does this declaration belongs to a function prototype?
  if (dyn_cast<ParmVarDecl>(D) != nullptr ||
      dyn_cast<FunctionDecl>(D) != nullptr) {
    int PIdx = -1;
    FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
    // Is this a parameter? Get the paramter index.
    if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
      // Okay, we got a request for a parameter.
      DeclContext *DC = PD->getParentFunctionOrMethod();
      assert(DC != nullptr);
      FD = dyn_cast<FunctionDecl>(DC);
      // Get the parameter index with in the function.
      for (unsigned i = 0; i < FD->getNumParams(); i++) {
        const ParmVarDecl *tmp = FD->getParamDecl(i);
        if (tmp == D) {
          PIdx = i;
          break;
        }
      }
    }

    // Get corresponding FVConstraint vars.
    std::set<FVConstraint *> *FunFVars = getFuncFVConstraints(FD, *this,
                                                              C, InFuncCtx);

    // This can happen when we have a call to function which is never
    // explicitly declared. In which case, we need to create a FVConstraint
    // for the FunctionDecl.
    if (FunFVars == nullptr) {
      // We are seeing a call to a function and its declaration
      // was never visited.
      VariableMap::iterator I =
          Variables.find(PersistentSourceLoc::mkPSL(FD, *C));
      assert (I == Variables.end() && "Never seen declaration.");

      FVConstraint *NewFV = new FVConstraint(FD, freeKey, CS, *C);
      std::set<FVConstraint *> TmpFV;
      TmpFV.insert(NewFV);
      insertNewFVConstraints(FD, TmpFV, C);
      FunFVars = getFuncFVConstraints(FD, *this, C, InFuncCtx);
    }

    assert (FunFVars != nullptr && "Unable to find function constraints.");

    if (PIdx != -1) {
      // This is a parameter, get all parameter constraints from FVConstraints.
      std::set<ConstraintVariable *> ParameterCons;
      ParameterCons.clear();
      for (auto fv : *FunFVars) {
        auto currParamConstraint = fv->getParamVar(PIdx);
        ParameterCons.insert(currParamConstraint.begin(),
                             currParamConstraint.end());
      }
      return ParameterCons;
    }

    std::set<ConstraintVariable*> TmpRet;
    TmpRet.insert(FunFVars->begin(), FunFVars->end());
    return TmpRet;
  } else {
    VariableMap::iterator I =
        Variables.find(PersistentSourceLoc::mkPSL(D, *C));
    if (I != Variables.end()) {
      return I->second;
    }
    return std::set<ConstraintVariable *>();
  }
}
// Given some expression E, what is the top-most constraint variable that
// E refers to? It could be none, in which case the returned set is empty. 
// Otherwise, the returned setcontains the constraint variable(s) that E 
// refers to.
std::set<ConstraintVariable *>
ProgramInfo::getVariable(Expr *E, ASTContext *C, bool InFuncCtx) {
  assert(persisted == false);

  // Get the constraint variables represented by this Expr.
  std::set<ConstraintVariable *> T;
  if (E)
    return getVariableHelper(E, T, C, InFuncCtx);
  else
    return T;
}

VariableMap &ProgramInfo::getVarMap() {
  return Variables;
}

bool ProgramInfo::isAValidPVConstraint(ConstraintVariable *C) {
  if (C != nullptr) {
    if (PVConstraint *PV = dyn_cast<PVConstraint>(C))
      return !PV->getCvars().empty();
  }
  return false;
}


std::set<FVConstraint *> *
    ProgramInfo::getExtFuncDeclConstraintSet(std::string FuncName) {
  if (ExternalFunctionDeclFVCons.find(FuncName) !=
      ExternalFunctionDeclFVCons.end()) {
    return &(ExternalFunctionDeclFVCons[FuncName]);
  }
  return nullptr;
}

std::set<FVConstraint *> *
    ProgramInfo::getExtFuncDefnConstraintSet(std::string FuncName) {
  if (ExternalFunctionDefnFVCons.find(FuncName) !=
      ExternalFunctionDefnFVCons.end()) {
    return &(ExternalFunctionDefnFVCons[FuncName]);
  }
  return nullptr;
}

std::set<FVConstraint *> *
ProgramInfo::getStaticFuncDefnConstraintSet(std::string FuncName,
                                            std::string FileName) {
  if (StaticFunctionDefnFVCons.find(FuncName) !=
      StaticFunctionDefnFVCons.end() &&
      StaticFunctionDefnFVCons[FuncName].find(FileName) !=
          StaticFunctionDefnFVCons[FuncName].end()) {
    return &(StaticFunctionDefnFVCons[FuncName][FileName]);
  }
  return nullptr;
}

std::set<FVConstraint *> *
ProgramInfo::getStaticFuncDeclConstraintSet(std::string FuncName,
                                            std::string FileName) {
  if (StaticFunctionDeclFVCons.find(FuncName) !=
      StaticFunctionDeclFVCons.end() &&
      StaticFunctionDeclFVCons[FuncName].find(FileName) !=
      StaticFunctionDeclFVCons[FuncName].end()) {
    return &(StaticFunctionDeclFVCons[FuncName][FileName]);
  }
  return nullptr;
}

bool ProgramInfo::applySubtypingRelation(ConstraintVariable *SrcCVar,
                                         ConstraintVariable *DstCVar) {
  bool Ret = false;
  PVConstraint *PvSrc = dyn_cast<PVConstraint>(SrcCVar);
  PVConstraint *PvDst = dyn_cast<PVConstraint>(DstCVar);

  if (!PvSrc->getCvars().empty() && !PvDst->getCvars().empty()) {

    CAtoms SrcCVars(PvSrc->getCvars());
    CAtoms DstCVars(PvDst->getCvars());

    // Cvars adjustment!
    // if the number of CVars is different, then adjust the number
    // of cvars to be same.
    if (SrcCVars.size() != DstCVars.size()) {
      auto &BigCvars = SrcCVars;
      auto &SmallCvars = DstCVars;
      if (SrcCVars.size() < DstCVars.size()) {
        BigCvars = DstCVars;
        SmallCvars = SrcCVars;
      }

      while (BigCvars.size() > SmallCvars.size()) {
        BigCvars.erase(BigCvars.begin());
      }
    }

    // Function subtyping only applies for the top level pointer.
    ConstAtom *OuterMostSrcVal = CS.getAssignment(*SrcCVars.begin());
    ConstAtom *OutputMostDstVal = CS.getAssignment(*DstCVars.begin());

    if (*OutputMostDstVal < *OuterMostSrcVal) {
      if (VarAtom *VA = dyn_cast<VarAtom>(*DstCVars.begin())) {
        CS.addConstraint(CS.createGeq(VA, OuterMostSrcVal));
      }
      Ret = true;
    }

    // For all the other pointer types they should be exactly same.
    // More details refer:
    // https://github.com/microsoft/checkedc-clang/issues/676.
    SrcCVars.erase(SrcCVars.begin());
    DstCVars.erase(DstCVars.begin());

    if (SrcCVars.size() == DstCVars.size()) {
      CAtoms::iterator SB = SrcCVars.begin();
      CAtoms::iterator DB = DstCVars.begin();

      while (SB != SrcCVars.end()) {
        ConstAtom *SVal = CS.getAssignment(*SB);
        ConstAtom *DVal = CS.getAssignment(*DB);
        // If these are not equal.
        if (*SVal < *DVal || *DVal < *SVal) {
          // Get the highest type.
          ConstAtom *FinalVal = *SVal < *DVal ? DVal : SVal;
          // Get the lowest constraint variable to change.
          Atom *Change = *SVal < *DVal ? *SB : *DB;
          if (VarAtom *VA = dyn_cast<VarAtom>(Change)) {
            CS.addConstraint(CS.createGeq(VA, FinalVal));
          }
          Ret = true;
        }
        SB++;
        DB++;
      }
    }
  }
  return Ret;
}

bool
ProgramInfo::applyFunctionSubtyping(std::set<ConstraintVariable *>
                                    &DefCVars,
                                    std::set<ConstraintVariable *>
                                    &DeclCVars) {
  // The subtyping rule for functions is:
  // T2 <: S2
  // S1 <: T1
  //--------------------
  // T1 -> T2 <: S1 -> S2
  // A way of interpreting this is that the type of a declaration argument
  // `S1` can be a subtype of a definition parameter type `T1`, and the type
  // of a definition return type `S2` can be a subtype of the declaration
  // expected type `T2`.

  bool Ret = false;
  auto &EnvMap = CS.getVariables();
  // Get the highest def and decl FVars.
  auto DefCVar = getHighestT<FVConstraint>(DefCVars, *this);
  auto DeclCVar = getHighestT<FVConstraint>(DeclCVars, *this);
  if (DefCVar != nullptr && DeclCVar != nullptr) {

    // Handle the return types.
    auto DefRetPvCons =
        getHighestT<PVConstraint>(DefCVar->getReturnVars(), *this);
    auto DeclRetPvCons =
        getHighestT<PVConstraint>(DeclCVar->getReturnVars(), *this);

    if (isAValidPVConstraint(DefRetPvCons) &&
        isAValidPVConstraint(DeclRetPvCons)) {
      // These are the constraint variables for top most pointers.
      auto HeadDefCVar = *(DefRetPvCons->getCvars().begin());
      auto HeadDeclCVar = *(DeclRetPvCons->getCvars().begin());
      // If the top-most constraint variable in the definition is WILD?
      // This is important in the cases of nested pointers.
      // i.e., int** foo().
      // If the top most pointer is WILD then we have to make
      // everything WILD.
      // We cannot have Ptr<int>*. However, we can have Ptr<int*>.

      // The function is returning WILD with in the body?
      if (CS.isWild(HeadDefCVar)) {
        // Make everything WILD.
        std::string Rsn = "Function Returning WILD within the body.";

        DefRetPvCons->constrainToWild(CS, Rsn, false);

        DeclRetPvCons->constrainToWild(CS, Rsn, false);

        Ret = true;
      } else if (CS.isWild(HeadDeclCVar)) {
        // If the declaration return type is WILD ?
        // Get the highest non-wild checked type.
        ConstraintVariable *BaseConsVar =
            ConstraintVariable::getHighestNonWildConstraint(
                DeclRetPvCons->getArgumentConstraints(), EnvMap, *this);
        PVConstraint *HighestNonWildCvar = DeclRetPvCons;
        if (isAValidPVConstraint(BaseConsVar))
          HighestNonWildCvar = dyn_cast<PVConstraint>(BaseConsVar);

        HeadDeclCVar = *(HighestNonWildCvar->getCvars().begin());

        auto DefAssignment = CS.getAssignment(HeadDefCVar);
        auto DeclAssignment = CS.getAssignment(HeadDeclCVar);

        // Okay, both declaration and definition are checked types.
        // Here we should apply the sub-typing relation.
        if (!CS.isWild(HeadDeclCVar) && *DefAssignment < *DeclAssignment) {
          // i.e., definition is not a subtype of declaration.
          // e.g., def = PTR and decl = ARR,
          //  here PTR is not a subtype of ARR
          // Oh, definition is more restrictive than declaration.
          // Promote the type of definition to higher type.
          Ret =
              applySubtypingRelation(HighestNonWildCvar, DefRetPvCons) ||
                  Ret;
        }

      } else {
        auto DefAssignment = CS.getAssignment(HeadDefCVar);
        auto DeclAssignment = CS.getAssignment(HeadDeclCVar);
        if (*DefAssignment < *DeclAssignment)
          Ret = applySubtypingRelation(DeclRetPvCons, DefRetPvCons) || Ret;
      }

    }

    // Handle the parameter types.
    if (DeclCVar->numParams() == DefCVar->numParams()) {
      std::set<ConstraintVariable *> ChangeCVars;
      // Compare parameters.
      for (unsigned i = 0; i < DeclCVar->numParams(); ++i) {
        auto DeclParam =
            getHighestT<PVConstraint>(DeclCVar->getParamVar(i), *this);
        auto DefParam =
            getHighestT<PVConstraint>(DefCVar->getParamVar(i), *this);
        if (isAValidPVConstraint(DeclParam) &&
            isAValidPVConstraint(DefParam)) {
          ChangeCVars.clear();
          auto HeadDefCVar = *(DefParam->getCvars().begin());
          auto HeadDeclCVar = *(DeclParam->getCvars().begin());

          if (!CS.isWild(HeadDefCVar)) {
            // The definition is not WILD.
            // So, we just need to check with the declaration.
            if (!CS.isWild(HeadDeclCVar)) {
              ChangeCVars.insert(DeclParam);
            } else {
              // The declaration is WILD. So, we need to iterate through all
              // the argument constraints and try to change them.
              // This is because if we only change the declaration,
              // as some caller is making it WILD, it will not propagate to
              // all the arguments. We need to explicitly change each of
              // the non-WILD arguments.
              for (auto ArgOrigCons : DeclParam->getArgumentConstraints()) {
                if (isAValidPVConstraint(ArgOrigCons)) {
                  PVConstraint *ArgPvCons =
                      dyn_cast<PVConstraint>(ArgOrigCons);
                  auto HeadArgCVar = *(ArgPvCons->getCvars().begin());
                  CAtoms DefPcVars(DefParam->getCvars());

                  // Is the top constraint variable WILD?
                  if (!CS.isWild(HeadArgCVar)) {
                    if (DefPcVars.size() > ArgPvCons->getCvars().size()) {

                      while (DefPcVars.size() >
                          ArgPvCons->getCvars().size())
                        DefPcVars.erase(DefPcVars.begin());

                      if (!CS.isWild(*(DefPcVars.begin())))
                        ChangeCVars.insert(ArgPvCons);
                    } else {
                      ChangeCVars.insert(ArgPvCons);
                    }
                  }
                }
              }
            }
            // Here we should apply the sub-typing relation
            // for all the toChageVars.
            for (auto ToChangeVar : ChangeCVars) {
              // i.e., declaration is not a subtype of definition.
              // e.g., decl = PTR and defn = ARR,
              //  here PTR is not a subtype of ARR
              // Oh, declaration is more restrictive than definition.
              // promote the type of declaration to higher type.
              Ret = applySubtypingRelation(DefParam, ToChangeVar) || Ret;
            }
          }
        }
      }
    }
  }
  return Ret;
}

bool
ProgramInfo::applyFunctionDefnDeclsConstraints(std::set<FVConstraint *>
                                                   &DefCVars,
                                               std::set<FVConstraint *>
                                                   &DeclCVars) {
  // We always set inside <: outside for parameters and
  // outside <: inside for returns
  for (auto *DeFV : DefCVars) {
    for (auto *DelFV : DeclCVars) {
      // DelFV is outside, DeFV is inside.
      // Rule for returns : outside <: inside for returns.
      constrainConsVarGeq(DelFV->getReturnVars(), DeFV->getReturnVars(), CS,
                          nullptr, Safe_to_Wild);

      assert (DeFV->numParams() == DelFV->numParams() &&
             "Definition and Declaration should have same "
             "number of paramters.");
      for (unsigned i=0; i<DeFV->numParams(); i++) {
        //Rule for parameters: inside <: outside for parameters.
        constrainConsVarGeq(DelFV->getParamVar(i), DeFV->getParamVar(i), CS,
                            nullptr, Wild_to_Safe);
      }
    }
  }

  return true;
}

bool ProgramInfo::handleFunctionSubtyping() {
  bool Ret = false;
  // Apply subtyping for external functions.
  for (auto &CurrFDef : ExternalFunctionDefnFVCons) {
    auto FuncName = CurrFDef.first;
    std::set<FVConstraint *> &DefFVCvars = CurrFDef.second;

    // It has declaration?
    if (ExternalFunctionDeclFVCons.find(FuncName) !=
        ExternalFunctionDeclFVCons.end()) {
      std::set<ConstraintVariable *> DefVars;
      DefVars.insert(DefFVCvars.begin(), DefFVCvars.end());

      std::set<ConstraintVariable *> DeclVars;
      DeclVars.insert(ExternalFunctionDeclFVCons[FuncName].begin(),
                      ExternalFunctionDeclFVCons[FuncName].end());

      Ret = applyFunctionSubtyping(DefVars, DeclVars) || Ret;

    }

  }

  // Apply subtyping for static functions.
  for (auto &StFDef : StaticFunctionDefnFVCons) {
    auto FuncName = StFDef.first;
    for (auto &StI : StFDef.second) {
      auto FileName = StI.first;
      std::set<FVConstraint *> &DefFVCvars = StI.second;
      if (StaticFunctionDeclFVCons.find(FuncName) !=
              StaticFunctionDeclFVCons.end() &&
          StaticFunctionDeclFVCons[FuncName].find(FileName) !=
              StaticFunctionDeclFVCons[FuncName].end()) {
        auto &DeclFVs = StaticFunctionDeclFVCons[FuncName][FileName];
        std::set<ConstraintVariable *> DefVars;
        DefVars.insert(DefFVCvars.begin(), DefFVCvars.end());

        std::set<ConstraintVariable *> DeclVars;
        DeclVars.insert(DeclFVs.begin(), DeclFVs.end());
        Ret = applyFunctionSubtyping(DefVars, DeclVars) || Ret;
      }
    }
  }

  return Ret;
}

bool ProgramInfo::addFunctionDefDeclConstraints() {
  bool Ret = true;
  for (auto &CurrFDef : ExternalFunctionDefnFVCons) {
    auto FuncName = CurrFDef.first;
    std::set<FVConstraint *> &DefFVCvars = CurrFDef.second;

    // It has declaration?
    if (ExternalFunctionDeclFVCons.find(FuncName) !=
        ExternalFunctionDeclFVCons.end()) {
      applyFunctionDefnDeclsConstraints(DefFVCvars,
                                        ExternalFunctionDeclFVCons[FuncName]);
    }

  }
  for (auto &StFDef : StaticFunctionDefnFVCons) {
    auto FuncName = StFDef.first;
    for (auto &StI : StFDef.second) {
      auto FileName = StI.first;
      std::set<FVConstraint *> &DefFVCvars = StI.second;
      if (StaticFunctionDeclFVCons.find(FuncName) !=
          StaticFunctionDeclFVCons.end() &&
          StaticFunctionDeclFVCons[FuncName].find(FileName) !=
              StaticFunctionDeclFVCons[FuncName].end()) {
        auto &DeclFVs = StaticFunctionDeclFVCons[FuncName][FileName];
        applyFunctionDefnDeclsConstraints(DefFVCvars,
                                          DeclFVs);
      }
    }
  }

  return Ret;
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
    const std::set<ConstraintVariable *> &S = I.second;
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
