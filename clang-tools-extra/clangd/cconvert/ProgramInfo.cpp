//=--ProgramInfo.cpp----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of ProgramInfo methods.
//===----------------------------------------------------------------------===//

#include "ProgramInfo.h"
#include "MappingVisitor.h"
#include "ConstraintBuilder.h"
#include "CCGlobalOptions.h"
#include "llvm/ADT/StringSwitch.h"
#include "clang/Lex/Lexer.h"
#include "GatherTool.h"
#include <sstream>

using namespace clang;

ProgramInfo::ProgramInfo() :
  freeKey(0), persisted(true) {
  ArrBoundsInfo = new ArrayBoundsInformation(*this);
  OnDemandFuncDeclConstraint.clear();
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

  O << "Dummy Declaration Constraint Variables\n";
  for (const auto &DeclCons : OnDemandFuncDeclConstraint) {
    O << "Func Name:" << DeclCons.first << " => ";
    const std::set<ConstraintVariable *> &S = DeclCons.second;
    for (const auto &J : S) {
      O << "[ ";
      J->print(O);
      O << " ]";
    }
    O << "\n";
  }
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
  // Dump on demand constraints.
  O << ", \"DummyFunctionConstraints\":[";
  AddComma = false;
  for (const std::pair<const std::string,
                       std::set<ConstraintVariable *>> &DeclCons :
      OnDemandFuncDeclConstraint) {
    if (AddComma) {
      O << ",";
    }
    O << "{\"functionName\":\"" << DeclCons.first << "\"";
    O << ", \"Constraints\":[";
    const std::set<ConstraintVariable *> &S = DeclCons.second;
    bool AddComma1 = false;
    for (const auto &J : S) {
      if (AddComma1) {
        O << ",";
      }
      J->dump_json(O);
      AddComma1 = true;
    }
    O << "]}";
    AddComma = true;
    O << "\n";
  }
  O << "]";
  O << "}";
}

// Given a ConstraintVariable V, retrieve all of the unique
// constraint variables used by V. If V is just a 
// PointerVariableConstraint, then this is just the contents 
// of 'vars'. If it either has a function pointer, or V is
// a function, then recurses on the return and parameter
// constraints.
static
CVars getVarsFromConstraint(ConstraintVariable *V, CVars T) {
  CVars R = T;

  if (PVConstraint *PVC = dyn_cast<PVConstraint>(V)) {
    R.insert(PVC->getCvars().begin(), PVC->getCvars().end());
   if (FVConstraint *FVC = PVC->getFV()) 
     return getVarsFromConstraint(FVC, R);
  } else if (FVConstraint *FVC = dyn_cast<FVConstraint>(V)) {
    for (const auto &C : FVC->getReturnVars()) {
      CVars tmp = getVarsFromConstraint(C, R);
      R.insert(tmp.begin(), tmp.end());
    }
    for (unsigned i = 0; i < FVC->numParams(); i++) {
      for (const auto &C : FVC->getParamVar(i)) {
        CVars tmp = getVarsFromConstraint(C, R);
        R.insert(tmp.begin(), tmp.end());
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
  Constraints::EnvironmentMap Env = CS.getVariables();
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

      CVars FoundVars;
      for (auto &C : I.second) {
        CVars tmp = getVarsFromConstraint(C, FoundVars);
        FoundVars.insert(tmp.begin(), tmp.end());
      }

      varC += FoundVars.size();
      for (const auto &N : FoundVars) {
        VarAtom *V = CS.getVar(N);
        assert(V != nullptr);
        auto K = Env.find(V);
        assert(K != Env.end());

        ConstAtom *CA = K->second;
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
                                     clang::QualType srcType) {

  // Check if both types are same.
  if (srcType == DstType)
    return true;

  const clang::Type *SrcTypePtr = srcType.getTypePtr();
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
  unsigned BothNotChar = SrcTypePtr->isCharType() ^ DstTypePtr->isCharType();
  unsigned BothNotInt =
      SrcTypePtr->isIntegerType() ^ DstTypePtr->isIntegerType();
  unsigned BothNotFloat =
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
        constrainEq(*I, *J, *this, nullptr, nullptr);
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
        constrainEq(*I, *J, *this, nullptr, nullptr);
        ++I;
        ++J;
      }
    }
  }

  if (!SeperateMultipleFuncDecls) {
    int Gap = 0;
    for (const auto &S : GlobalFunctionSymbols) {
      std::string Fname = S.first;
      std::set<GlobFuncConstraintType> P = S.second;

      if (P.size() > 1) {
        std::set<GlobFuncConstraintType>::iterator I = P.begin();
        std::set<GlobFuncConstraintType>::iterator J = P.begin();
        ++J;

        while (J != P.end()) {
          FVConstraint *P1 = (*I).second;
          FVConstraint *P2 = (*J).second;

          if (P2->hasBody()) { // skip over decl with fun body
            Gap = 1;
            ++J;
            continue;
          }
          // Constrain the return values to be equal.
          if (!P1->hasBody() && !P2->hasBody()) {
            constrainEq(P1->getReturnVars(), P2->getReturnVars(), *this,
                        nullptr, nullptr);

            // Constrain the parameters to be equal, if the parameter arity is
            // the same. If it is not the same, constrain both to be wild.
            if (P1->numParams() == P2->numParams()) {
              for (unsigned i = 0; i < P1->numParams(); i++) {
                constrainEq(P1->getParamVar(i), P2->getParamVar(i), *this,
                            nullptr, nullptr);
              }

            } else {
              // It could be the case that P1 or P2 is missing a prototype, in
              // which case we don't need to constrain anything.
              if (P1->hasProtoType() && P2->hasProtoType()) {
                // Nope, we have no choice. Constrain everything to wild.
                std::string rsn = "Return value of function:" + P1->getName();
                P1->constrainTo(CS, CS.getWild(), rsn, true);
                P2->constrainTo(CS, CS.getWild(), rsn, true);
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
      std::string UnkSymbol = U.first;
      auto GlobFuncIterator =
          GlobalFunctionSymbols.find(UnkSymbol);
      assert(GlobFuncIterator != GlobalFunctionSymbols.end());
      const std::set<GlobFuncConstraintType> &Gs = (*GlobFuncIterator).second;

      for (const auto &GIterator : Gs) {
        auto G = GIterator.second;
        for (const auto &U : G->getReturnVars()) {
          std::string Rsn = "Return value of function:" +
                            (*GlobFuncIterator).first;
          U->constrainTo(CS, CS.getWild(), Rsn, true);
        }

        std::string rsn = "Inner pointer of a parameter to external function.";
        for (unsigned i = 0; i < G->numParams(); i++)
          for (const auto &PVar : G->getParamVar(i)) {
            if (PVConstraint *PVC = dyn_cast<PVConstraint>(PVar)) {
              // Remove the first constraint var and make all the internal
              // constraint vars WILD. For more details, refer Section 5.3 of
              // http://www.cs.umd.edu/~mwh/papers/checkedc-incr.pdf
              CVars C = PVC->getCvars();
              if (!C.empty())
                C.erase(C.begin());
              for (auto cVar : C)
                CS.addConstraint(CS.createEq(CS.getVar(cVar),
                                             CS.getWild(), rsn));
            } else {
              PVar->constrainTo(CS, CS.getWild(), rsn,true);
            }
          }
      }
    }
  }

  return true;
}

void
ProgramInfo::insertIntoGlobalFunctions(FunctionDecl *FD,
                                       std::set<GlobFuncConstraintType>
                                           &ToAdd) {
  std::string Fname = FD->getNameAsString();
  auto GlobFuncIterator =
      GlobalFunctionSymbols.find(Fname);

  if (GlobFuncIterator == GlobalFunctionSymbols.end()) {
      GlobalFunctionSymbols.insert(std::pair<std::string,
                                           std::set<GlobFuncConstraintType>>
                                       (Fname, ToAdd));
  } else {
    (*GlobFuncIterator).second.insert(ToAdd.begin(), ToAdd.end());
  }
}

void ProgramInfo::insertIntoGlobalFunctions(FunctionDecl *FD, ASTContext *C,
                                            FVConstraint *ToAdd) {
  std::set<GlobFuncConstraintType> TmpVars;
  TmpVars.clear();
  TmpVars.insert(std::make_pair(getUniqueDeclKey(FD, C), ToAdd));
  insertIntoGlobalFunctions(FD, TmpVars);
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
  
  // Add this to the map of global symbols. 
  std::set<GlobFuncConstraintType> ToAdd;
  // Get the constraint variable directly.
  std::set<ConstraintVariable *> K;
  VariableMap::iterator I = Variables.find(PersistentSourceLoc::mkPSL(F, *C));
  if (I != Variables.end()) {
    K = I->second;
  }
  for (const auto &J : K) {
    if (FVConstraint *FJ = dyn_cast<FVConstraint>(J))
      ToAdd.insert(std::make_pair(getUniqueDeclKey(F, C),FJ));
  }

  assert(ToAdd.size() > 0);

  insertIntoGlobalFunctions(F, ToAdd);

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

// This function takes care of handling multiple declarations of an external
// function across multiple files. Specifically, it will create a mapping of
// definition and declaration, and sets a flag to indicate that rewrites have
// to done twice inorder to correctly rewrite the prototype of all the function
// declarations
void ProgramInfo::performDefnDeclarationAssociation(FunctionDecl *FD,
                                                    ASTContext *C) {
  std::string FuncKey =  getUniqueDeclKey(FD, C);
  // If this is global function and not previously processed?
  // Look into external declarations in other C files.
  auto &DefDeclMap = CS.getFuncDefnDeclMap();
  if (FD->isGlobal() && !DefDeclMap.hasKey(FuncKey) &&
      !DefDeclMap.hasValue(FuncKey)) {
    std::string FuncName = FD->getNameAsString();
    bool HasBody = (FD->isThisDeclarationADefinition() && FD->hasBody());
    bool Handled = false;
    // Check all the global function and when a declaration is found.
    // Add it as the declaration for the current definition
    if (GlobalFunctionSymbols.find(FuncName) != GlobalFunctionSymbols.end()) {
      for (auto &SymPair : GlobalFunctionSymbols[FuncName]) {
        if (SymPair.first != FuncKey) {
          if (SymPair.second->hasBody() != HasBody || !HasBody) {
            // Declarations across multiple files and should be rewritten.
            MultipleRewrites = true;
          }
          // This is a definition and we have seen a declaration.
          if (HasBody && !SymPair.second->hasBody()) {
            CS.getFuncDefnDeclMap().set(FuncKey, SymPair.first);
            Handled = true;
          } else if (SymPair.second->hasBody() && !HasBody) {
            // This is first external declaration and we have seen a definition
            if (!CS.getFuncDefnDeclMap().hasKey(SymPair.first))
                CS.getFuncDefnDeclMap().set(SymPair.first, FuncKey);
            Handled = true;
          } else {
             assert((HasBody != SymPair.second->hasBody() ||
                    !HasBody) &&
                    "Multiple definitions of a single function.");
          }
          if (Handled)
            break;
        }
      }
    }
  }
}

// For each pointer type in the declaration of D, add a variable to the
// constraint system for that pointer type.
bool ProgramInfo::addVariable(DeclaratorDecl *D, DeclStmt *St, ASTContext *C) {
  assert(persisted == false);
  PersistentSourceLoc PLoc = 
    PersistentSourceLoc::mkPSL(D, *C);
  assert(PLoc.valid());
  // What is the nature of the constraint that we should be adding? This is 
  // driven by the type of Decl. 
  //  - Decl is a pointer-type VarDecl - we will add a PVConstraint
  //  - Decl has type Function - we will add a FVConstraint
  //  If Decl is both, then we add both. If it has neither, then we add
  //  neither.
  // We only add a PVConstraint or an FVConstraint if the set at 
  // Variables[PLoc] does not contain one already. This allows either 
  // PVConstraints or FVConstraints declared at the same physical location
  // in the program to implicitly alias.

  const Type *Ty = nullptr;
  if (VarDecl *VD = dyn_cast<VarDecl>(D))
    Ty = VD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
  else if (FieldDecl *FD = dyn_cast<FieldDecl>(D))
    Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
  else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D))
    Ty = UD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
  else
    llvm_unreachable("unknown decl type");
  
  FVConstraint *F = nullptr;
  PVConstraint *P = nullptr;
  
  if (Ty->isPointerType() || Ty->isArrayType()) 
    // Create a pointer value for the type.
    P = new PVConstraint(D, freeKey, CS, *C);

  // Only create a function type if the type is a base Function type. The case
  // for creating function pointers is handled above, with a PVConstraint that
  // contains a FVConstraint.
  if (Ty->isFunctionType()) 
    // Create a function value for the type.
    F = new FVConstraint(D, freeKey, CS, *C);

  std::set<ConstraintVariable *> &S = Variables[PLoc];
  bool NewFunction = false;

  if (F != nullptr && !hasConstraintType<FVConstraint>(S)) {
    // insert the function constraint only if it doesn't exist
    NewFunction = true;
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
    FunctionDecl *UD = dyn_cast<FunctionDecl>(D);
    std::string FuncKey =  getUniqueDeclKey(UD, C);
    // This is a definition. Create a constraint variable
    // and save the mapping between definition and declaration.
    if (UD->isThisDeclarationADefinition() && UD->hasBody()) {
      CS.getFuncDefnVarMap()[FuncKey].insert(F);
      // This is a definition.
      // Get the declaration and store the unique key mapping.
      FunctionDecl *FDecl = getDeclaration(UD);
      if (FDecl != nullptr) {
        std::string fDeclKey = getUniqueDeclKey(FDecl, C);
        CS.getFuncDefnDeclMap().set(FuncKey, fDeclKey);
      } else {
        performDefnDeclarationAssociation(UD, C);
      }
    } else {
      // This is a declaration, just save the constraint variable.
      CS.getFuncDeclVarMap()[FuncKey].insert(F);
      performDefnDeclarationAssociation(UD, C);
    }
  }

  if (P != nullptr && !hasConstraintType<PVConstraint>(S)) {
    // If there is no pointer constraint in this location
    // insert it.
    S.insert(P);
  }

  // Did we create a function and it is a newly added function
  if (F && NewFunction) {
    // If we did, then we need to add some additional stuff to Variables. 
    //  * A mapping from the parameters PLoc to the constraint variables for
    //    the parameters.
    FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
    assert(FD != nullptr);
    // We just created this, so they should be equal.
    assert(FD->getNumParams() == F->numParams());
    for (unsigned i = 0; i < FD->getNumParams(); i++) {
      ParmVarDecl *PVD = FD->getParamDecl(i);
      std::set<ConstraintVariable *> S = F->getParamVar(i);
      if (S.size()) {
        PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(PVD, *C);
        Variables[PSL].insert(S.begin(), S.end());
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
      C->constrainTo(CS, CS.getWild(), Rsn);

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
        std::set<uint32_t> C = PVC->getCvars();
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
    if (UO->getOpcode() == UO_Deref) {
      for (const auto &CV : T) {
        if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
          // Subtract one from this constraint. If that generates an empty 
          // constraint, then, don't add it 
          std::set<uint32_t> C = PVC->getCvars();
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
        } else {
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
    // Explore the three exprs individually.
    std::set<ConstraintVariable *> T;
    std::set<ConstraintVariable *> R;
    T = getVariableHelper(CO->getCond(), V, C, Ifc);
    R.insert(T.begin(), T.end());
    T = getVariableHelper(CO->getLHS(), V, C, Ifc);
    R.insert(T.begin(), T.end());
    T = getVariableHelper(CO->getRHS(), V, C, Ifc);
    R.insert(T.begin(), T.end());
    return R;
  } else if (StringLiteral *exr = dyn_cast<StringLiteral>(E)) {
    // If this is a string literal. i.e., "foo".
    // We create a new constraint variable and constraint it to an Nt_array.
    std::set<ConstraintVariable *> T;
    // Create a new constraint var number.
    CVars V;
    V.insert(freeKey);
    CS.getOrCreateVar(freeKey);
    freeKey++;
    ConstraintVariable *newC = new PointerVariableConstraint(V,
                                                             "const char*",
                                                             exr->getBytes(),
                                                             nullptr,
                                                             false,
                                                             false, "");
    // Constraint the newly created variable to NTArray.
    newC->constrainTo(CS, CS.getNTArr());
    T.insert(newC);
    return T;

  } else {
    return std::set<ConstraintVariable *>();
  }
}

std::map<std::string, std::set<ConstraintVariable *>>&
ProgramInfo::getOnDemandFuncDeclConstraintMap() {
  return OnDemandFuncDeclConstraint;
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

std::set<ConstraintVariable *>&
ProgramInfo::getOnDemandFuncDeclarationConstraint(FunctionDecl *D,
                                                  ASTContext *C) {
  std::string DeclKey = getUniqueFuncKey(D, C);
  if (OnDemandFuncDeclConstraint.find(DeclKey) ==
      OnDemandFuncDeclConstraint.end()) {
    const Type *Ty = D->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    assert (!(Ty->isPointerType() || Ty->isArrayType()) && "");
    assert(Ty->isFunctionType() && "");
    FVConstraint *F = new FVConstraint(D, freeKey, CS, *C);
    // Set has body is false, as this is for function declaration.
    F->setHasBody(false);
    // Insert into function declarations which will help in linking.
    insertIntoGlobalFunctions(D, C, F);
    OnDemandFuncDeclConstraint[DeclKey].insert(F);
    // Insert into declaration map.
    CS.getFuncDeclVarMap()[DeclKey].insert(F);
  }
  return OnDemandFuncDeclConstraint[DeclKey];
}

std::set<ConstraintVariable *>&
ProgramInfo::getFuncDefnConstraints(FunctionDecl *D, ASTContext *C) {
  std::string FuncKey = getUniqueDeclKey(D, C);

  if (D->isThisDeclarationADefinition() && D->hasBody()) {
    return CS.getFuncDefnVarMap()[FuncKey];
  } else {
    // If this is function declaration? see if we have definition.
    // Have we seen a definition of this function?
    if (CS.getFuncDefnDeclMap().hasValue(FuncKey)) {
      auto FdefKey = *(CS.getFuncDefnDeclMap().valueMap().at(FuncKey).begin());
      return  CS.getFuncDefnVarMap()[FdefKey];
    }
    return CS.getFuncDeclVarMap()[FuncKey];
  }
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

// Given a decl, return the variables for the constraints of the Decl.
std::set<ConstraintVariable *>
ProgramInfo::getVariableOnDemand(Decl *D, ASTContext *C,
                                 bool InFuncCtx) {
  assert(persisted == false);
  VariableMap::iterator I =
      Variables.find(PersistentSourceLoc::mkPSL(D, *C));
  if (I != Variables.end()) {
    // If we are looking up a variable, and that variable is a parameter
    // variable, or return value then we should see if we're looking this up
    // in the context of a function or not.
    // If we are not, then we should find a declaration.
    ParmVarDecl *PD = nullptr;
    FunctionDecl *FuncDef = nullptr;
    FunctionDecl *FuncDecl = nullptr;
    // Get the function declaration and definition.
    if (D != nullptr && dyn_cast<FunctionDecl>(D)) {
      FuncDecl = getDeclaration(dyn_cast<FunctionDecl>(D));
      FuncDef = getDefinition(dyn_cast<FunctionDecl>(D));
    }
    int PIdx = -1;
    if (PD = dyn_cast<ParmVarDecl>(D)) {
      // Okay, we got a request for a parameter.
      DeclContext *DC = PD->getParentFunctionOrMethod();
      assert(DC != nullptr);
      FunctionDecl *FD = dyn_cast<FunctionDecl>(DC);
      // Get the parameter index with in the function.
      for (unsigned i = 0; i < FD->getNumParams(); i++) {
        const ParmVarDecl *tmp = FD->getParamDecl(i);
        if (tmp == D) {
          PIdx = i;
          break;
        }
      }

      // Get declaration and definition.
      FuncDecl = getDeclaration(FD);
      FuncDef = getDefinition(FD);
      
      // If this is an external function and we are unable
      // to find the body. Get the FD object from the parameter.
      if (!FuncDef && !FuncDecl) {
        FuncDecl = FD;
      }
      assert(PIdx >= 0 && "Got request for invalid parameter");
    }
    if (FuncDecl || FuncDef || PIdx != -1) {
      // If we are asking for the constraint variable of a function
      // and that function is an external function then use declaration.
      if (dyn_cast<FunctionDecl>(D) && FuncDef == nullptr) {
        FuncDef = FuncDecl;
      }
      // This means either we got a
      // request for function return value or parameter.
      if (InFuncCtx) {
        assert(FuncDef != nullptr && "Requesting for in-context "
                                            "constraints, but there is no "
                                            "definition for this function");
        // Return the constraint variable that belongs to the
        // function definition.
        return getVariable(D, C, FuncDef, PIdx);
      } else {
        if (FuncDecl == nullptr) {
          // We need constraint variable
          // with in the function declaration,
          // but there is no declaration
          // get on demand declaration.
          std::set<ConstraintVariable *> &FvConstraints =
              getOnDemandFuncDeclarationConstraint(FuncDef, C);
          if (PIdx != -1) {
            // This is a parameter.
            std::set<ConstraintVariable *> ParameterCons;
            ParameterCons.clear();
            assert(FvConstraints.size() && "Unable to find on demand "
                                           "fv constraints.");
            // Get all parameters from all the FVConstraints.
            for (auto fv : FvConstraints) {
              auto currParamConstraint =
                  (dyn_cast<FunctionVariableConstraint>(fv))->getParamVar(PIdx);
              ParameterCons.insert(currParamConstraint.begin(),
                                          currParamConstraint.end());
            }
            return ParameterCons;
          }
          return FvConstraints;
        } else {
          // Return the variable with in
          // the function declaration.
          return getVariable(D, C, FuncDecl, PIdx);
        }
      }
      // We got a request for function return or parameter
      // but we failed to handle the request.
      assert(false && "Invalid state reached.");
    }
    // neither parameter or return value.
    // just return the original constraint.
    return I->second;
  } else {
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

std::set<ConstraintVariable *> *
    ProgramInfo::getFuncDeclConstraintSet(std::string FuncDefKey) {
  std::set<ConstraintVariable *> *DeclCVarsPtr = nullptr;
  auto &DefnDeclMap = CS.getFuncDefnDeclMap();
  auto &DeclConstrains = CS.getFuncDeclVarMap();
  // See if we do not have constraint variables for declaration.
  if (DefnDeclMap.hasKey(FuncDefKey)) {
    auto DeclKey = DefnDeclMap.keyMap().at(FuncDefKey);
    // If this has a declaration constraint?
    // then fetch the constraint.
    if (DeclConstrains.find(DeclKey) != DeclConstrains.end()) {
      DeclCVarsPtr = &(DeclConstrains[DeclKey]);
    }
  } else {
    // no? then check the ondemand declarations.
    auto &OnDemandMap = getOnDemandFuncDeclConstraintMap();
    if (OnDemandMap.find(FuncDefKey) != OnDemandMap.end()) {
      DeclCVarsPtr = &(OnDemandMap[FuncDefKey]);
    }
  }
  return DeclCVarsPtr;
}

bool ProgramInfo::applySubtypingRelation(ConstraintVariable *SrcCVar,
                                         ConstraintVariable *DstCVar) {
  bool Ret = false;
  PVConstraint *PvSrc = dyn_cast<PVConstraint>(SrcCVar);
  PVConstraint *PvDst = dyn_cast<PVConstraint>(DstCVar);

  if (!PvSrc->getCvars().empty() && !PvDst->getCvars().empty()) {

    CVars SrcCVars(PvSrc->getCvars());
    CVars DstCVars(PvDst->getCvars());

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

      while (BigCvars.size() > SmallCvars.size())
        BigCvars.erase(*(BigCvars.begin()));
    }

    // Function subtyping only applies for the top level pointer.
    ConstAtom *OuterMostSrcVal = CS.getAssignment(*SrcCVars.begin());
    ConstAtom *OutputMostDstVal = CS.getAssignment(*DstCVars.begin());

    if (*OutputMostDstVal < *OuterMostSrcVal) {
      CS.addConstraint(CS.createEq(CS.getVar(*DstCVars.begin()),
                                   OuterMostSrcVal));
      Ret = true;
    }

    // For all the other pointer types they should be exactly same.
    // More details refer:
    // https://github.com/microsoft/checkedc-clang/issues/676.
    SrcCVars.erase(SrcCVars.begin());
    DstCVars.erase(DstCVars.begin());

    if (SrcCVars.size() == DstCVars.size()) {
      CVars::iterator SB = SrcCVars.begin();
      CVars::iterator DB = DstCVars.begin();

      while (SB != SrcCVars.end()) {
        ConstAtom *SVal = CS.getAssignment(*SB);
        ConstAtom *DVal = CS.getAssignment(*DB);
        // If these are not equal.
        if (*SVal < *DVal || *DVal < *SVal) {
          // Get the highest type.
          ConstAtom *FinalVal = *SVal < *DVal ? DVal : SVal;
          // Get the lowest constraint variable to change.
          VarAtom *Change = *SVal < *DVal ? CS.getVar(*SB) : CS.getVar(*DB);
          CS.addConstraint(CS.createEq(Change, FinalVal));
          Ret = true;
        }
        SB++;
        DB++;
      }
    }
  }
  return Ret;
}

bool ProgramInfo::handleFunctionSubtyping() {
  // The subtyping rule for functions is:
  // T2 <: S2
  // S1 <: T1
  //--------------------
  // T1 -> T2 <: S1 -> S2
  // A way of interpreting this is that the type of a declaration argument `S1` can be a
  // subtype of a definition parameter type `T1`, and the type of a definition
  // return type `S2` can be a subtype of the declaration expected type `T2`.
  //
  bool Ret = false;
  auto &EnvMap = CS.getVariables();
  for (auto &CurrFDef : CS.getFuncDefnVarMap()) {
    // Get the key for the function definition.
    auto DefKey = CurrFDef.first;
    std::set<ConstraintVariable *> &DefCVars = CurrFDef.second;

    std::set<ConstraintVariable *> *DeclCVarsPtr =
        getFuncDeclConstraintSet(DefKey);

    if (DeclCVarsPtr != nullptr) {
      // If we have declaration constraint variables?
      std::set<ConstraintVariable *> &DeclCVars = *DeclCVarsPtr;
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
            for (const auto &B : DefRetPvCons->getCvars())
              CS.addConstraint(CS.createEq(CS.getOrCreateVar(B),
                                           CS.getWild(), Rsn));

            for (const auto &B : DeclRetPvCons->getCvars())
              CS.addConstraint(CS.createEq(CS.getOrCreateVar(B),
                                           CS.getWild(), Rsn));

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
                      CVars DefPcVars(DefParam->getCvars());

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
    if (Eq *EC = dyn_cast<Eq>(currC)) {
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
          ConstraintDisjointSet.PtrSourceMap[ck] =
              (PersistentSourceLoc*)(&(I.first));
        }
      }
      if (FVConstraint *FV = dyn_cast<FVConstraint>(CV)) {
        for (auto PV : FV->getReturnVars()) {
          if (PVConstraint *RPV = dyn_cast<PVConstraint>(PV)) {
            for (auto ck : RPV->getCvars()) {
              ConstraintDisjointSet.PtrSourceMap[ck] =
                  (PersistentSourceLoc*)(&(I.first));
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
