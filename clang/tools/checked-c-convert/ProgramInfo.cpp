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
#include "ConstraintVariables.h"
#include "llvm/ADT/StringSwitch.h"
#include "clang/Lex/Lexer.h"
#include <sstream>

using namespace clang;

ProgramInfo::ProgramInfo() :
  freeKey(0), persisted(true) {
  ArrBoundsInfo = new ArrayBoundsInformation(*this);
  OnDemandFuncDeclConstraint.clear();
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
    for (const ConstraintVariable *J : S) {
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
    for (const ConstraintVariable *J : S) {
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
    for (const ConstraintVariable *J : S) {
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
    for (const ConstraintVariable *J : S) {
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
    O << "Merge multiple function declaration:" << MergeMultipleFuncDecls
      << "\n";
    O << "Sound handling of var args functions:" << HandleVARARGS << "\n";
  }
  std::map<std::string, std::tuple<int, int, int, int, int> > FilesToVars;
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
  // If not only summary then dump everything.
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
  // Both are pointers? check their pointee.
  if (SrcPtrTypePtr && DstPtrTypePtr)
    return isExplicitCastSafe(DstPtrTypePtr->getPointeeType(),
                              SrcPtrTypePtr->getPointeeType());
  // Only one of them is pointer?
  if (SrcPtrTypePtr || DstPtrTypePtr)
    return false;

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
        constrainEq(*I, *J, *this);
        ++I;
        ++J;
      }
    }
  }

  for (const auto &S : GlobalSymbols) {
    std::string Fname = S.first;
    std::set<FVConstraint *> P = S.second;
    
    if (P.size() > 1) {
      std::set<FVConstraint *>::iterator I = P.begin();
      std::set<FVConstraint *>::iterator J = P.begin();
      ++J;
      
      while (J != P.end()) {
        FVConstraint *P1 = *I;
        FVConstraint *P2 = *J;

        // Constrain the return values to be equal
        if (!P1->hasBody() && !P2->hasBody() && MergeMultipleFuncDecls) {
          constrainEq(P1->getReturnVars(), P2->getReturnVars(), *this);

          // Constrain the parameters to be equal, if the parameter arity is
          // the same. If it is not the same, constrain both to be wild.
          if (P1->numParams() == P2->numParams()) {
            for ( unsigned i = 0;
                  i < P1->numParams();
                  i++)
            {
              constrainEq(P1->getParamVar(i), P2->getParamVar(i), *this);
            } 

          } else {
            // It could be the case that P1 or P2 is missing a prototype, in
            // which case we don't need to constrain anything.
            if (P1->hasProtoType() && P2->hasProtoType()) {
              // Nope, we have no choice. Constrain everything to wild.
              std::string Rsn = "Return value of function:" + P1->getName();
              P1->constrainTo(CS, CS.getWild(), Rsn, true);
              P2->constrainTo(CS, CS.getWild(), Rsn, true);
            }
          }
        }
        ++I;
        ++J;
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
      std::map<std::string, std::set<FVConstraint *>>::iterator I =
        GlobalSymbols.find(UnkSymbol);
      assert(I != GlobalSymbols.end());
      const std::set<FVConstraint *> &Gs = (*I).second;

      for (const auto &G : Gs) {
        for (const auto &U : G->getReturnVars()) {
          std::string Rsn = "Return value of function:" + (*I).first;
          U->constrainTo(CS, CS.getWild(), Rsn, true);
        }

        for (unsigned i = 0; i < G->numParams(); i++)
          for (const auto &PVar : G->getParamVar(i)) {
            if (PVConstraint *PVC = dyn_cast<PVConstraint>(PVar)) {
              // Remove the first constraint var and make all the internal
              // constraint vars WILD. For more details, refer Section 5.3 of
              // http://www.cs.umd.edu/~mwh/papers/checkedc-incr.pdf.
              CVars C = PVC->getCvars();
              if (!C.empty())
                C.erase(C.begin());
              for (auto cVar : C)
                CS.addConstraint(CS.createEq(CS.getVar(cVar), CS.getWild()));
            } else {
              PVar->constrainTo(CS, CS.getWild(), true);
            }
          }
      }
    }
  }

  return true;
}

void ProgramInfo::seeFunctionDecl(FunctionDecl *F, ASTContext *C) {
  if (!F->isGlobal())
    return;

  // Track if we've seen a body for this function or not.
  std::string Fname = F->getNameAsString();
  if (!ExternFunctions[Fname])
    ExternFunctions[Fname] = (F->isThisDeclarationADefinition() &&
                              F->hasBody());
  
  // Add this to the map of global symbols. 
  std::set<FVConstraint *> Add;
  // Get the constraint variable directly.
  std::set<ConstraintVariable *> K;
  VariableMap::iterator I = Variables.find(PersistentSourceLoc::mkPSL(F, *C));
  if (I != Variables.end()) {
    K = I->second;
  }
  for (const auto &J : K)
    if (FVConstraint *FJ = dyn_cast<FVConstraint>(J))
      Add.insert(FJ);

  assert(Add.size() > 0);

  std::map<std::string, std::set<FVConstraint *>>::iterator it =
    GlobalSymbols.find(Fname);
  
  if (it == GlobalSymbols.end()) {
    GlobalSymbols.insert(std::pair<std::string, std::set<FVConstraint *>>
      (Fname, Add));
  } else {
    (*it).second.insert(Add.begin(), Add.end());
  }

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
    GlobalSymbols.find(fn);
  
  if (it == GlobalSymbols.end()) {
    std::set<GlobalSymbol*> N;
    N.insert(GF);
    GlobalSymbols.insert(std::pair<std::string, std::set<GlobalSymbol*> >
      (fn, N));
  } else {
    (*it).second.insert(GF);
  }*/
}

void ProgramInfo::seeGlobalDecl(clang::VarDecl *G) {

}

// Populate Variables, VarDeclToStatement, RVariables, and DepthMap with
// AST data structures that correspond do the data stored in PDMap and
// ReversePDMap.
void ProgramInfo::enterCompilationUnit(ASTContext &Context) {
  assert(persisted == true);
  // Get a set of all of the PersistentSourceLoc's we need to fill in
  std::set<PersistentSourceLoc> P;
  //for (auto I : PersistentVariables)
  //  P.insert(I.first);

  // Resolve the PersistentSourceLoc to one of Decl,Stmt,Type.
  MappingVisitor V(P, Context);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  for (const auto &D : TUD->decls())
    V.TraverseDecl(D);
  MappingResultsType Res = V.getResults();
  SourceToDeclMapType PSLtoDecl = Res.first;

  // Re-populate VarDeclToStatement.
  VarDeclToStatement = Res.second;

  persisted = false;
  return;
}

// Remove any references we maintain to AST data structure pointers.
// After this, the Variables, VarDeclToStatement, RVariables, and DepthMap
// should all be empty.
void ProgramInfo::exitCompilationUnit() {
  assert(persisted == false);
  VarDeclToStatement.clear();
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
    // Insert the function constraint only if it doesn't exist.
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
    // and save the mapping between defintion and declaration.
    if (UD->isThisDeclarationADefinition() && UD->hasBody()) {
      CS.getFuncDefnVarMap()[FuncKey].insert(F);
      // This is a definition.
      // Get the declartion and store the unique key mapping.
      FunctionDecl *FDecl = getDeclaration(UD);
      if (FDecl != nullptr) {
        CS.getFuncDefnDeclMap()[FuncKey] = getUniqueDeclKey(FDecl, C);
      }
    } else {
      // This is a declaration, just save the constraint variable.
      CS.getFuncDeclVarMap()[FuncKey].insert(F);
    }
  }

  if (P != nullptr && !hasConstraintType<PVConstraint>(S)) {
    // If there is no pointer constraint in this location insert it.
    S.insert(P);
  }

  // Did we create a function and it is a newly added function?
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
  if (!Rewriter::isRewritable(D->getLocation())) 
    for (const auto &C : S)
      C->constrainTo(CS, CS.getWild());

  return true;
}

bool ProgramInfo::getDeclStmtForDecl(Decl *D, DeclStmt *&St) {
  assert(persisted == false);
  auto I = VarDeclToStatement.find(D);
  if (I != VarDeclToStatement.end()) {
    St = I->second;
    return true;
  } else
    return false;
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
    std::set<ConstraintVariable *> T1 = getVariableHelper(BO->getLHS(), V,
                                                          C, Ifc);
    std::set<ConstraintVariable *> T2 = getVariableHelper(BO->getRHS(), V,
                                                          C, Ifc);
    T1.insert(T2.begin(), T2.end());
    return T1;
  } else if (ArraySubscriptExpr *AE = dyn_cast<ArraySubscriptExpr>(E)) {
    // In an array subscript, we want to do something sort of similar to taking
    // the address or doing a dereference. 
    std::set<ConstraintVariable *> T = getVariableHelper(AE->getBase(), V,
                                                         C, Ifc);
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
            tmp.insert(new PVConstraint(C, PVC->getTy(), PVC->getName(), b,
                                        a, c, d));
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
    // Call expression should always get out-of context constraint variable.
    Ifc = false;
    // Here, we need to look up the target of the call and return the
    // constraints for the return value of that function.
    Decl *D = CE->getCalleeDecl();
    if (D == nullptr) {
      // There are a few reasons that we couldn't get a decl. For example,
      // the call could be done through an array subscript. 
      Expr *CalledExpr = CE->getCallee();
      std::set<ConstraintVariable *> tmp = getVariableHelper(CalledExpr, V,
                                                             C, Ifc);
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
    // If this is a string literal. i.e., "foo"
    // we create a new constraint variable and constraint it to an Nt_array.
    std::set<ConstraintVariable *> T;
    // Create a new constraint var number.
    CVars V;
    V.insert(freeKey);
    CS.getOrCreateVar(freeKey);
    freeKey++;
    ConstraintVariable *newC = new PointerVariableConstraint(V, "const char*",
                                                             exr->getBytes(),
                                                             nullptr, false,
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
  auto PL = PersistentSourceLoc::mkPSL(D, *C);
  std::string FileName =
      PL.getFileName() + ":" + std::to_string(PL.getLineNo());
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
    OnDemandFuncDeclConstraint[DeclKey].insert(F);
    // Insert into declaration map.
    CS.getFuncDeclVarMap()[DeclKey].insert(F);
  }
  return OnDemandFuncDeclConstraint[DeclKey];
}
std::set<ConstraintVariable *>
ProgramInfo::getVariable(clang::Decl *D, clang::ASTContext *C,
                         FunctionDecl *FD, int PIdx) {
  // If this is a parameter.
  if (PIdx >= 0) {
    // Get the parameter index of the
    // requested function declaration.
    D = FD->getParamDecl(PIdx);
  } else {
    // This is the return value of the function.
    D = FD;
  }
  VariableMap::iterator I = Variables.find(PersistentSourceLoc::mkPSL(D, *C));
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
  VariableMap::iterator I = Variables.find(PersistentSourceLoc::mkPSL(D, *C));
  if (I != Variables.end()) {
    // If we are looking up a variable, and that variable is a
    // parameter variable, or return value then we should see if we're looking
    // this up in the context of a function or not. If we are not, then we
    // should find a declaration.
    ParmVarDecl *PD = nullptr;
    FunctionDecl *FuncDef = nullptr;
    FunctionDecl *FuncDec = nullptr;
    // Get the function declaration and definition.
    if (D != nullptr && dyn_cast<FunctionDecl>(D)) {
      FuncDec = getDeclaration(dyn_cast<FunctionDecl>(D));
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

      // Get declaration and definition
      FuncDec = getDeclaration(FD);
      FuncDef = getDefinition(FD);
      
      // If this is an external function and we are unable
      // to find the body. Get the FD object from the parameter.
      if (!FuncDef && !FuncDec) {
        FuncDec = FD;
      }
      assert(PIdx >= 0 && "Got request for invalid parameter");
    }
    if (FuncDec || FuncDef || PIdx != -1) {
      // If we are asking for the constraint variable of a function
      // and that function is an external function, then use declaration.
      if (dyn_cast<FunctionDecl>(D) && FuncDef == nullptr) {
        FuncDef = FuncDec;
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
        if (FuncDec == nullptr) {
          // We need constraint variable with in the function declaration,
          // but there is no declaration get on demand declaration.
          std::set<ConstraintVariable *> &FvConstraints =
              getOnDemandFuncDeclarationConstraint(FuncDef, C);
          if (PIdx != -1) {
            // This is a parameter.
            std::set<ConstraintVariable *> ParameterCons;
            ParameterCons.clear();
            assert(FvConstraints.size() && "Unable to find on demand "
                                           "fv constraints.");
            // Get all parameters from all the FVConstraints.
            for (auto Fv : FvConstraints) {
              auto CurrPCons =
                  (dyn_cast<FunctionVariableConstraint>(Fv))->getParamVar(PIdx);
              ParameterCons.insert(CurrPCons.begin(), CurrPCons.end());
            }
            return ParameterCons;
          }
          return FvConstraints;
        } else {
          // Return the variable with in the function declaration
          return getVariable(D, C, FuncDec, PIdx);
        }
      }
      // We got a request for function return or parameter
      // but we failed to handle the request.
      assert(false && "Invalid state reached.");
    }
    // Neither parameter or return value.
    // Just return the original constraint.
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

  // Get the constraint variables represented by this Expr
  std::set<ConstraintVariable *> T;
  if (E)
    return getVariableHelper(E, T, C, InFuncCtx);
  else
    return T;
}

VariableMap &ProgramInfo::getVarMap() {
  return Variables;
}

std::set<ConstraintVariable *> *
    ProgramInfo::getFuncDeclConstraintSet(std::string FuncDefKey) {
  std::set<ConstraintVariable *> *DeclCVarsPtr = nullptr;
  auto &DefnDeclMap = CS.getFuncDefnDeclMap();
  auto &DeclConstrains = CS.getFuncDeclVarMap();
  // See if we do not have constraint variables for declaration
  if (DefnDeclMap.find(FuncDefKey) != DefnDeclMap.end()) {
    auto DeclKey = DefnDeclMap[FuncDefKey];
    // If this has a declaration constraint?
    // then fetch the constraint.
    if (DeclConstrains.find(DeclKey) != DeclConstrains.end()) {
      DeclCVarsPtr = &(DeclConstrains[DeclKey]);
    }
  } else {
    // No? then check the ondemand declarations.
    auto &OnDemandMap = getOnDemandFuncDeclConstraintMap();
    if (OnDemandMap.find(FuncDefKey) != OnDemandMap.end()) {
      DeclCVarsPtr = &(OnDemandMap[FuncDefKey]);
    }
  }
  return DeclCVarsPtr;
}

bool ProgramInfo::applySubtypingRelation(ConstraintVariable *SrcCVar,
                                         ConstraintVariable *DstCVar) {
  bool retVal = false;
  PVConstraint *PvSrc = dyn_cast<PVConstraint>(SrcCVar);
  PVConstraint *PvDst = dyn_cast<PVConstraint>(DstCVar);

  if (!PvSrc->getCvars().empty() && !PvDst->getCvars().empty()) {
    auto &EnvMap = CS.getVariables();

    CVars SrcCVars(PvSrc->getCvars());
    CVars DstCVars(PvDst->getCvars());

    // Function subtyping only applies for the top level pointer.
    ConstAtom *OuterMostSrcVal = EnvMap[CS.getOrCreateVar(*SrcCVars.begin())];
    ConstAtom *OutputMostDstVal = EnvMap[CS.getOrCreateVar(*DstCVars.begin())];

    if (*OutputMostDstVal < *OuterMostSrcVal) {
      CS.addConstraint(CS.createEq(CS.getVar(*DstCVars.begin()),
                                   OuterMostSrcVal));
      retVal = true;
    }

    // For all the other pointer types they should be exactly same.
    // Refer: https://github.com/microsoft/checkedc-clang/issues/676
    SrcCVars.erase(SrcCVars.begin());
    DstCVars.erase(DstCVars.begin());

    if (SrcCVars.size() == DstCVars.size()) {
      CVars::iterator SB = SrcCVars.begin();
      CVars::iterator DB = DstCVars.begin();

      while (SB != SrcCVars.end()) {
        ConstAtom *SVal = EnvMap[CS.getVar(*SB)];
        ConstAtom *DVal = EnvMap[CS.getVar(*DB)];
        // if these are not equal.
        if (*SVal < *DVal || *DVal < *SVal) {
          // Get the highest type.
          ConstAtom *FinalVal = *SVal < *DVal ? DVal : SVal;
          // Get the lowest constraint variable to change.
          VarAtom *Change = *SVal < *DVal ? CS.getVar(*SB) : CS.getVar(*DB);
          CS.addConstraint(CS.createEq(Change, FinalVal));
          retVal = true;
        }
        SB++;
        DB++;
      }
    }
  }
  return retVal;
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
      auto DefCVar = dyn_cast<FVConstraint>(getHighest(DefCVars, *this));
      auto DeclCVar = dyn_cast<FVConstraint>(getHighest(DeclCVars, *this));

      // Handle the return types.
      auto DefRetType = getHighest(DefCVar->getReturnVars(), *this);
      auto DeclRetType = getHighest(DeclCVar->getReturnVars(), *this);

      if (DefRetType->hasWild(EnvMap)) {
        // The function is returning WILD with in the body.
        // Make the declaration type also WILD.
        PVConstraint *ChangeVar = dyn_cast<PVConstraint>(DeclRetType);
        for (const auto &B : ChangeVar->getCvars())
          CS.addConstraint(CS.createEq(CS.getOrCreateVar(B), CS.getWild()));

        Ret = true;
      } else {
        ConstraintVariable *HighestNonWildCvar = DeclRetType;
        // If the declaration return type is WILD ?
        if (DeclRetType->hasWild(EnvMap)) {
          // Get the highest non-wild checked type.
          if (PVConstraint *DeclPvCons = dyn_cast<PVConstraint>(DeclRetType))
            HighestNonWildCvar =
                ConstraintVariable::getHighestNonWildConstraint(
                    DeclPvCons->getArgumentConstraints(), EnvMap, *this);
          if (HighestNonWildCvar == nullptr)
            HighestNonWildCvar = DeclRetType;
        }
        // Okay, both declaration and definition are checked types.
        // here we should apply the sub-typing relation.
        if (!HighestNonWildCvar->hasWild(EnvMap) &&
            DefRetType->isLt(*HighestNonWildCvar, *this)) {
          // i.e., definition is not a subtype of declaration.
          // e.g., def = PTR and decl = ARR,
          //  here PTR is not a subtype of ARR
          // Oh, definition is more restrictive than declaration.
          // promote the type of definition to higher type.
          Ret = applySubtypingRelation(HighestNonWildCvar, DefRetType) || Ret;
        }
      }

      // Handle the parameter types.
      if (DeclCVar->numParams() == DefCVar->numParams()) {
        std::set<ConstraintVariable *> ChangeCVars;
        // Compare parameters.
        for (unsigned i = 0; i < DeclCVar->numParams(); ++i) {
          auto DeclParam = getHighest(DeclCVar->getParamVar(i), *this);
          auto DefParam = getHighest(DefCVar->getParamVar(i), *this);
          ChangeCVars.clear();
          if (!DefParam->hasWild(EnvMap)) {
            // The declaration is not WILD.
            // So, we just need to check with the declaration.
            if (!DeclParam->hasWild(EnvMap)) {
              ChangeCVars.insert(DeclParam);
            } else if (PVConstraint *declPVar =
                           dyn_cast<PVConstraint>(DeclParam)) {
              // The declaration is WILD. So, we need to iterate through all
              // the argument constraints and try to change them.
              // This is because if we only change the declaration,
              // as some caller is making it WILD, it will not propagate to
              // all the arguments.
              // We need to explicitly change each of the non-WILD arguments.
              for (auto ArgCVar : declPVar->getArgumentConstraints()) {
                if (!ArgCVar->hasWild(EnvMap))
                  ChangeCVars.insert(ArgCVar);
              }
            }

            // Here we should apply the sub-typing relation for all
            // the toChageVars.
            for (auto ToChangeCVar : ChangeCVars) {
              // i.e., declaration is not a subtype of definition.
              // e.g., decl = PTR and defn = ARR,
              //  here PTR is not a subtype of ARR
              // Oh, declaration is more restrictive than definition.
              // promote the type of declaration to higher type.
              Ret = applySubtypingRelation(DefParam, ToChangeCVar) || Ret;
            }
          }
        }
      }
    }

  }
  return Ret;
}