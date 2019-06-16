//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Implementation of ProgramInfo methods.
//===----------------------------------------------------------------------===//
#include "ProgramInfo.h"
#include "MappingVisitor.h"
#include "ConstraintBuilder.h"
#include "llvm/ADT/StringSwitch.h"
#include "clang/Lex/Lexer.h"
#include <sstream>

using namespace clang;

void ProgramInfo::print(raw_ostream &O) const {
  CS.print(O);
  O << "\n";

  O << "Constraint Variables\n";
  for( const auto &I : Variables ) {
    PersistentSourceLoc L = I.first;
    const std::set<ConstraintVariable*> &S = I.second;
    L.print(O);
    O << "=>";
    for(const auto &J : S) {
      O << "[ ";
      J->print(O);
      O << " ]";
    }
    O << "\n";
  }
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
void ProgramInfo::print_stats(std::set<std::string> &F, raw_ostream &O) {
  std::map<std::string, std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned> > filesToVars;
  Constraints::EnvironmentMap env = CS.getVariables();

  // First, build the map and perform the aggregation.
  for (auto &I : Variables) {
    std::string fileName = I.first.getFileName();
    if (F.count(fileName)) {
      unsigned varC = 0;
      unsigned pC = 0;
      unsigned aC = 0;
      unsigned ntAC = 0;
      unsigned wC = 0;

      auto J = filesToVars.find(fileName);
      if (J != filesToVars.end())
        std::tie(varC, pC, aC, ntAC, wC) = J->second;

      CVars foundVars;
      for (auto &C : I.second) {
        CVars tmp = getVarsFromConstraint(C, foundVars);
        foundVars.insert(tmp.begin(), tmp.end());
        }

      varC += foundVars.size();
      for (const auto &N : foundVars) {
        VarAtom *V = CS.getVar(N);
        assert(V != NULL);
        auto K = env.find(V);
        assert(K != env.end());

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

      filesToVars[fileName] = std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned>(varC, pC, aC, ntAC, wC);
    }
  }

  // Then, dump the map to output.

  O << "file|#constraints|#ptr|#arr|#ntarr|#wild\n";
  for (const auto &I : filesToVars) {
    int v, p, a, nta, w;
    std::tie(v, p, a, nta, w) = I.second;
    O << I.first << "|" << v << "|" << p << "|" << a << "|" << nta << "|" << w;
    O << "\n";
  }
}

// Check the equality of VTy and UTy. There are some specific rules that
// fire, and a general check is yet to be implemented. 
bool ProgramInfo::checkStructuralEquality(std::set<ConstraintVariable*> V, 
                                          std::set<ConstraintVariable*> U,
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

  if (D->isPointerType() == S->isPointerType())
    return true;

  return false;
}

bool ProgramInfo::isExternOkay(std::string ext) {
  return llvm::StringSwitch<bool>(ext)
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
    std::set<ConstraintVariable*> C = V.second;

    if (C.size() > 1) {
      std::set<ConstraintVariable*>::iterator I = C.begin();
      std::set<ConstraintVariable*>::iterator J = C.begin();
      ++J;

      while (J != C.end()) {
        constrainEq(*I, *J, *this);
        ++I;
        ++J;
      }
    }
  }

  for (const auto &S : GlobalSymbols) {
    std::string fname = S.first;
    std::set<FVConstraint*> P = S.second;
    
    if (P.size() > 1) {
      std::set<FVConstraint*>::iterator I = P.begin();
      std::set<FVConstraint*>::iterator J = P.begin();
      ++J;
      
      while (J != P.end()) {
        FVConstraint *P1 = *I;
        FVConstraint *P2 = *J;

        // Constrain the return values to be equal
        if (!P1->hasBody() && !P2->hasBody() && mergeMultipleDeclarations) {
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
              P1->constrainTo(CS, CS.getWild(), true);
              P2->constrainTo(CS, CS.getWild(), true);
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
      std::map<std::string, std::set<FVConstraint*> >::iterator I =
        GlobalSymbols.find(UnkSymbol);
      assert(I != GlobalSymbols.end());
      const std::set<FVConstraint*> &Gs = (*I).second;

      for (const auto &G : Gs) {
        for(const auto &U : G->getReturnVars()) {
          U->constrainTo(CS, CS.getWild(), true);
        }

        for(unsigned i = 0; i < G->numParams(); i++) 
          for(const auto &U : G->getParamVar(i)) 
            U->constrainTo(CS, CS.getWild(), true);
      }
    }
  }

  return true;
}

void ProgramInfo::seeFunctionDecl(FunctionDecl *F, ASTContext *C) {
  if (!F->isGlobal())
    return;

  // Track if we've seen a body for this function or not.
  std::string fn = F->getNameAsString();
  if (!ExternFunctions[fn])
    ExternFunctions[fn] = (F->isThisDeclarationADefinition() && F->hasBody());
  
  // Add this to the map of global symbols. 
  std::set<FVConstraint*> toAdd;
  std::set<ConstraintVariable*> K = getVariable(F, C);
  for (const auto &J : K)
    if(FVConstraint *FJ = dyn_cast<FVConstraint>(J))
      toAdd.insert(FJ);

  assert(toAdd.size() > 0);

  std::map<std::string, std::set<FVConstraint*> >::iterator it = 
    GlobalSymbols.find(fn);
  
  if (it == GlobalSymbols.end()) {
    GlobalSymbols.insert(std::pair<std::string, std::set<FVConstraint*> >
      (fn, toAdd));
  } else {
    (*it).second.insert(toAdd.begin(), toAdd.end());
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
  assert(PFV != NULL);
  const FVConstraint *FVC = dyn_cast<FVConstraint>(PFV);
  assert(FVC != NULL);

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
  MappingResultsType res = V.getResults();
  SourceToDeclMapType PSLtoDecl = res.first;

  // Re-populate VarDeclToStatement.
  VarDeclToStatement = res.second;

  persisted = false;
  return;
}

// Remove any references we maintain to AST data structure pointers.
// After this, the Variables, VarDeclToStatement, RVariables, and DepthMap
// should all be empty.
void ProgramInfo::exitCompilationUnit() {
  assert(persisted == false);
  VarDeclToStatement.clear();
  // remove all the references.
  IdentifiedArrayDecls.clear();
  AllocationBasedSizeExprs.clear();
  persisted = true;
  return;
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

  std::set<ConstraintVariable*> &S = Variables[PLoc];
  bool found = false;
  for (const auto &I : S)
    if (isa<FVConstraint>(I))
      found = true;

  if (found == false && F != nullptr)
    Variables[PLoc].insert(F);
  found = false;

  for (const auto &I : S)
    if (isa<PVConstraint>(I))
      found = true;

  if (found == false && P != nullptr)
    Variables[PLoc].insert(P);

  // Did we create a function?
  if (F) {
    // If we did, then we need to add some additional stuff to Variables. 
    //  * A mapping from the parameters PLoc to the constraint variables for
    //    the parameters.
    FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
    assert(FD != nullptr);
    // We just created this, so they should be equal.
    assert(FD->getNumParams() == F->numParams());
    for (unsigned i = 0; i < FD->getNumParams(); i++) {
      ParmVarDecl *PVD = FD->getParamDecl(i);
      std::set<ConstraintVariable*> S = F->getParamVar(i); 
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
                                bool                            ifc)
{
  E = E->IgnoreParenImpCasts();
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
    return getVariable(DRE->getDecl(), C, ifc);
  } else if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
    return getVariable(ME->getMemberDecl(), C, ifc);
  } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
    std::set<ConstraintVariable*> T1 = getVariableHelper(BO->getLHS(), V, C, ifc);
    std::set<ConstraintVariable*> T2 = getVariableHelper(BO->getRHS(), V, C, ifc);
    T1.insert(T2.begin(), T2.end());
    return T1;
  } else if (ArraySubscriptExpr *AE = dyn_cast<ArraySubscriptExpr>(E)) {
    // In an array subscript, we want to do something sort of similar to taking
    // the address or doing a dereference. 
    std::set<ConstraintVariable *> T = getVariableHelper(AE->getBase(), V, C, ifc);
    std::set<ConstraintVariable*> tmp;
    for (const auto &CV : T) {
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
        // Subtract one from this constraint. If that generates an empty 
        // constraint, then, don't add it 
        std::set<uint32_t> C = PVC->getCvars();
        if(C.size() > 0) {
          C.erase(C.begin());
          if (C.size() > 0) {
            bool a = PVC->getArrPresent();
            bool c = PVC->getItypePresent();
            std::string d = PVC->getItype();
            FVConstraint *b = PVC->getFV();
            tmp.insert(new PVConstraint(C, PVC->getTy(), PVC->getName(), b, a, c, d));
          }
        }
      }
    }

    T.swap(tmp);
    return T;
  } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
    std::set<ConstraintVariable *> T = 
      getVariableHelper(UO->getSubExpr(), V, C, ifc);
   
    std::set<ConstraintVariable*> tmp;
    if (UO->getOpcode() == UO_Deref) {
      for (const auto &CV : T) {
        if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
          // Subtract one from this constraint. If that generates an empty 
          // constraint, then, don't add it 
          std::set<uint32_t> C = PVC->getCvars();
          if(C.size() > 0) {
            C.erase(C.begin());
            if (C.size() > 0) {
              bool a = PVC->getArrPresent();
              FVConstraint *b = PVC->getFV();
              bool c = PVC->getItypePresent();
              std::string d = PVC->getItype();
              tmp.insert(new PVConstraint(C, PVC->getTy(), PVC->getName(), b, a, c, d));
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
    return getVariableHelper(IE->getSubExpr(), V, C, ifc);
  } else if (ParenExpr *PE = dyn_cast<ParenExpr>(E)) {
    return getVariableHelper(PE->getSubExpr(), V, C, ifc);
  } else if (CHKCBindTemporaryExpr *CBE = dyn_cast<CHKCBindTemporaryExpr>(E)) {
    return getVariableHelper(CBE->getSubExpr(), V, C, ifc);
  } else if (CallExpr *CE = dyn_cast<CallExpr>(E)) {
    // Here, we need to look up the target of the call and return the
    // constraints for the return value of that function.
    Decl *D = CE->getCalleeDecl();
    if (D == nullptr) {
      // There are a few reasons that we couldn't get a decl. For example,
      // the call could be done through an array subscript. 
      Expr *CalledExpr = CE->getCallee();
      std::set<ConstraintVariable*> tmp = getVariableHelper(CalledExpr, V, C, ifc);
      std::set<ConstraintVariable*> T;

      for (ConstraintVariable *C : tmp) {
        if (FVConstraint *FV = dyn_cast<FVConstraint>(C)) {
          T.insert(FV->getReturnVars().begin(), FV->getReturnVars().end());
        } else if(PVConstraint *PV = dyn_cast<PVConstraint>(C)) {
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
      std::set<ConstraintVariable*> CS = getVariable(FD, C, ifc);
      std::set<ConstraintVariable*> TR;
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
    std::set<ConstraintVariable*> T;
    std::set<ConstraintVariable*> R;
    T = getVariableHelper(CO->getCond(), V, C, ifc);
    R.insert(T.begin(), T.end());
    T = getVariableHelper(CO->getLHS(), V, C, ifc);
    R.insert(T.begin(), T.end());
    T = getVariableHelper(CO->getRHS(), V, C, ifc);
    R.insert(T.begin(), T.end());
    return R;
  } else {
    return std::set<ConstraintVariable*>();
  }
}

// Given a decl, return the variables for the constraints of the Decl.
std::set<ConstraintVariable*>
ProgramInfo::getVariable(Decl *D, ASTContext *C, bool inFunctionContext) {
  assert(persisted == false);
  VariableMap::iterator I = Variables.find(PersistentSourceLoc::mkPSL(D, *C));
  if (I != Variables.end()) {
    // If we are looking up a variable, and that variable is a parameter variable,
    // then we should see if we're looking this up in the context of a function or
    // not. If we are not, then we should find a declaration 
    if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D)) {
      if (!inFunctionContext) {
        // We need to do 2 things:
        //  - Look up a forward declaration of the function for this parameter.
        //  - Map 'D', which is the ith parameter of Parent, to the ith parameter
        //    of any forward declaration.
        //
        // If such a forward declaration doesn't exist, then we can back off. 

        const DeclContext *DC = PD->getParentFunctionOrMethod();
        assert(DC != nullptr);
        if(const FunctionDecl *Parent = dyn_cast<FunctionDecl>(DC)) {
          // Check that the current function declaration doesn't have a body.
          bool hasbody = false; 
          const FunctionDecl *oFD = nullptr;
          if (Parent->hasBody(oFD) && oFD == Parent)
            hasbody = true; 

          // This ParmVarDecl belongs to a method declaration that has a body,
          // and, our caller asked for a non-method declaration variable. Let's
          // see if we can find one by looking through the re-declarations of
          // Parent. 
          if (hasbody) {
            // Let's look through all the re-declarations of Parent. 
            const FunctionDecl *fwdDecl = nullptr;
            for (const auto &RD : Parent->redecls()) {
              if (RD != Parent) {
                fwdDecl = RD;
                break;
              }
            }

            if (fwdDecl) {
              // We found one! Let's figure out the index that D has in Parent,
              // then get that decl from fwdDecl and look it up in Variables
              // by PSL, then return it. 
              int idx = -1;
              
              for (unsigned i = 0; i < Parent->getNumParams(); i++) {
                const ParmVarDecl *tmp = Parent->getParamDecl(i);

                if (tmp == D) {
                  idx = i;
                  break;
                }
              }

              assert(idx >= 0);

              const ParmVarDecl *otherDecl = fwdDecl->getParamDecl(idx);
              I = Variables.find(PersistentSourceLoc::mkPSL(otherDecl, *C));
              assert(I != Variables.end());
            }
          }
        }
      }
    }
    return I->second;
  } else {
    return std::set<ConstraintVariable*>();
  }
}
// Given some expression E, what is the top-most constraint variable that
// E refers to? It could be none, in which case the returned set is empty. 
// Otherwise, the returned setcontains the constraint variable(s) that E 
// refers to.
std::set<ConstraintVariable*>
ProgramInfo::getVariable(Expr *E, ASTContext *C, bool inFunctionContext) {
  assert(persisted == false);

  // Get the constraint variables represented by this Expr
  std::set<ConstraintVariable*> T;
  if (E)
    return getVariableHelper(E, T, C, inFunctionContext);
  else
    return T;
}

bool ProgramInfo::insertPotentialArrayVar(Decl *var) {
  return IdentifiedArrayDecls.insert(var).second;
}

bool ProgramInfo::isIdentifiedArrayVar(Decl *toCheckVar) {
  return IdentifiedArrayDecls.find(toCheckVar) != IdentifiedArrayDecls.end();
}

bool ProgramInfo::addAllocationBasedSizeExpr(Decl *targetVar, Expr *sizeExpr) {
  assert(isIdentifiedArrayVar(targetVar) && "The provided variable is not an array variable");
  return AllocationBasedSizeExprs[targetVar].insert(sizeExpr).second;
}

void ProgramInfo::printArrayVarsAndSizes(llvm::raw_ostream &O) {
  O << "\n\nArray Variables and Sizes\n";
  for (const auto &currEl: AllocationBasedSizeExprs) {
    O << "Variable:";
    currEl.first->dump(O);
    O << ", Possible Sizes:\n";
    for (auto sizeExpr: currEl.second) {
      sizeExpr->dump(O);
      O << "\n";
    }
  }
}