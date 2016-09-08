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

using namespace clang;
using namespace llvm;

static const Type *getNextTy(const Type *Ty) {
  if (const PointerType *PT = dyn_cast<PointerType>(Ty))
    return PT->getPointeeType().getTypePtr()->getUnqualifiedDesugaredType();
  else
    return Ty;
}

void ProgramInfo::print(raw_ostream &O) const {
  CS.print(O);
  O << "\n";

  O << "Constraint Variables\n";
  for (const auto &I : PersistentRVariables) {
    VarAtom *V = CS.getVar(I.first);
    V->print(O);
    O << "=>";
    I.second.print(O);
    O << "\n";
  }

  return;
}

// Print out statistics of constraint variables on a per-file basis.
void ProgramInfo::print_stats(raw_ostream &O) {
  std::map<std::string, std::tuple<int, int, int, int> > filesToVars;
  Constraints::EnvironmentMap env = CS.getVariables();

  // First, build the map and perform the aggregation.
  for (const auto &I : PersistentRVariables) {
    const std::string fileName = I.second.getFileName();
    int varC = 0;
    int pC = 0;
    int aC = 0;
    int wC = 0;

    auto J = filesToVars.find(fileName);
    if (J != filesToVars.end()) 
      std::tie(varC, pC, aC, wC) = J->second;

    varC += 1;

    VarAtom *V = CS.getVar(I.first);
    assert(V != NULL);
    auto K = env.find(V);
    assert(K != env.end());

    ConstAtom *CA = K->second;
    switch (CA->getKind()) {
      case Atom::A_Arr:
        aC += 1;
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

    filesToVars[fileName] = std::tuple<int, int, int, int>(varC, pC, aC, wC);
  }

  // Then, dump the map to output.

  O << "file|#constraints|#ptr|#arr|#wild\n";
  for (const auto &I : filesToVars) {
    int v, p, a, w;
    std::tie(v, p, a, w) = I.second;
    O << I.first << "|" << v << "|" << p << "|" << a << "|" << w;
    O << "\n";
  }
}

bool ProgramInfo::checkStructuralEquality(uint32_t V, uint32_t U) {
  // TODO: implement structural equality checking.
  return false;
}

bool ProgramInfo::link() {

  // For every global symbol in all the global symbols that we have found
  // go through and apply rules for whether they are functions or variables.
  if (Verbose)
    errs() << "Linking!\n";

  for (const auto &S : GlobalSymbols) {
    // First, extract out the function symbols from S. 
    std::set<GlobalFunctionSymbol*> funcs;
    std::set<GlobalVariableSymbol*> vars;
    for (const auto &U : S.second)
      if (GlobalFunctionSymbol *K = dyn_cast<GlobalFunctionSymbol>(U))
        funcs.insert(K);
      else if (GlobalVariableSymbol *V = dyn_cast<GlobalVariableSymbol>(U))
        vars.insert(V);

    // Then, iterate over each of the function symbols F1=F,F2=F+1 found for this 
    // symbol. What we want to do is for a sequence of constraint variables on
    // F1,F2, set the constraint variables for F1(v0,vi,vN) and F2(v0,vj,vN) to
    // be equal to each other, i.e. to enter a series of constraints of the form
    // vi == vj.
    //
    // For example, consider a function that has been forward declared named 
    // d1 with the signature void d1(int **c); There will be two sets of constraint
    // variables in the system, one at the site of forward declaration and one at
    // the site of function definition, like this:
    //
    // void d1(int *q_0 * q_1 c);
    //
    // void d1(int *q_2 * q_3 c) { *c = 0; }
    //
    // What we want to do is set q_0 == q_2 and q_1 == q_3. 
    // To do that, we need to get the constraints for each parmvar decl position
    // individually and set them equal.
    // individually and set them equal, pairwise. 
    for (std::set<GlobalFunctionSymbol*>::iterator I = funcs.begin();
      I != funcs.end(); ++I) {
      std::set<GlobalFunctionSymbol*>::iterator J = I;
      J++;
      if (J != funcs.end()) {
        std::set<uint32_t> &rVars1 = (*I)->getReturns();
        std::set<uint32_t> &rVars2 = (*J)->getReturns();
        if (rVars1.size() == rVars2.size()) {

          for (std::set<uint32_t>::iterator V1 = rVars1.begin(), V2 = rVars2.begin();
            V1 != rVars1.end() && V2 != rVars2.end(); ++V1, ++V2)
            CS.addConstraint(CS.createEq(
              CS.getOrCreateVar(*V1), CS.getOrCreateVar(*V2)));
        } else {
          // Nothing makes sense because this means that the types of two 
          // functions with the same name is different. Constrain 
          // everything to top.
          if (Verbose)
            errs() << "Constraining return value for symbol " << (*I)->getName()
            << ", " << (*J)->getName() 
            << " to top because return value arity does not match\n";

          for (const auto &V : rVars1)
            CS.addConstraint(CS.createEq(
              CS.getOrCreateVar(V), CS.getWild()));
          for (const auto &V : rVars2)
            CS.addConstraint(CS.createEq(
              CS.getOrCreateVar(V), CS.getWild()));
        }
        
        std::vector<std::set<uint32_t> > &pVars1 = (*I)->getParams();
        std::vector<std::set<uint32_t> > &pVars2 = (*J)->getParams();
        if (pVars1.size() == pVars2.size()) {

          for (std::vector<std::set<uint32_t> >::iterator V1 = pVars1.begin(),
            V2 = pVars2.begin();
            V1 != pVars1.end() && V2 != pVars2.end();
            ++V1, ++V2)
          {
            std::set<uint32_t> pv1 = *V1;
            std::set<uint32_t> pv2 = *V2;

            if (pv1.size() == pv2.size()) {
              for (std::set<uint32_t>::iterator V1 = pv1.begin(), V2 = pv2.begin();
                V1 != pv1.end() && V2 != pv2.end(); ++V1, ++V2)
                CS.addConstraint(CS.createEq(
                  CS.getOrCreateVar(*V1), CS.getOrCreateVar(*V2)));
            } else {
              if(Verbose)
                errs() << "Constraining return value for symbol " << (*I)->getName()
                  << ", " << (*J)->getName()
                  << " to top because return value arity does not match\n";

              for (const auto &V : pv1)
                CS.addConstraint(CS.createEq(
                  CS.getOrCreateVar(V), CS.getWild()));
              for (const auto &V : pv2)
                CS.addConstraint(CS.createEq(
                  CS.getOrCreateVar(V), CS.getWild()));
            }
          }
        } else {
          // Nothing makes sense because this means the parameter types of
          // the functions are different. Constrain everything to top.
          if (Verbose) 
            errs() << "Constraining parameters for symbol " << (*I)->getName()
              << ", " << (*J)->getName()
              << " to top because parameter arity does not match\n";
          
          for (const auto &VV : pVars1)
            for (const auto &V : VV)
              CS.addConstraint(CS.createEq(
                CS.getOrCreateVar(V), CS.getWild()));

          for (const auto &VV : pVars2)
            for (const auto &V : VV)
              CS.addConstraint(CS.createEq(
                CS.getOrCreateVar(V), CS.getWild()));
        }
      }
    }

    // Do the same as above, but in a simpler case where we only need to 
    // constrain according to the type of the global variable. 
    for (std::set<GlobalVariableSymbol*>::iterator I = vars.begin();
      I != vars.end(); ++I) {

    }
  }

  // For every global function that is an unresolved external, constrain 
  // its parameter types to be wild.
  for (const auto &U : ExternFunctions) {
    // If we've seen this symbol, but never seen a body for it, constrain
    // everything about it.
    if (U.second == false) {
      std::string UnkSymbol = U.first;
      std::map<std::string, std::set<GlobalSymbol*> >::iterator I =
        GlobalSymbols.find(UnkSymbol);
      assert(I != GlobalSymbols.end());
      std::set<GlobalSymbol*> Gs = (*I).second;

      for (const auto &G : Gs) {
        if (GlobalFunctionSymbol *GFS = dyn_cast<GlobalFunctionSymbol>(G)) {
          for (const auto &V : GFS->getReturns())
            CS.addConstraint(CS.createEq(CS.getOrCreateVar(V), CS.getWild()));

          for (const auto &U : GFS->getParams())
            for (const auto &V : U)
              CS.addConstraint(CS.createEq(CS.getOrCreateVar(V), CS.getWild()));
        }
      }
    }
  }

  return true;
}

void ProgramInfo::seeFunctionDecl(FunctionDecl *F, ASTContext *C) {
  if (!F->isGlobal())
    return;

  // Look up the constraint variables for the return type and parameter 
  // declarations of this function, if any.
  std::string fn = F->getNameAsString();
  std::set<uint32_t> returnVars;
  std::vector<std::set<uint32_t> > parameterVars(F->getNumParams());
  PersistentSourceLoc PLoc = PersistentSourceLoc::mkPSL(F, *C);
  int i = 0;

  getVariable(F, returnVars, C);
  for (auto &I : F->params())
    getVariable(I, parameterVars[i++], C);

  assert(PLoc.valid());
  GlobalFunctionSymbol *GF = 
    new GlobalFunctionSymbol(fn, PLoc, parameterVars, returnVars);

  // Track if we've seen a body for this function or not.
  if (!ExternFunctions[fn])
    ExternFunctions[fn] = (F->isThisDeclarationADefinition() && F->hasBody());

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
  }
}

void ProgramInfo::seeGlobalDecl(clang::VarDecl *G) {

}

void ProgramInfo::addRecordDecl(clang::RecordDecl *R, ASTContext *C) {
  for (const auto &D : R->fields()) 
    if (D->getType()->isPointerType()) 
      addVariable(D, NULL, C);
}

// Populate Variables, VarDeclToStatement, RVariables, and DepthMap with
// AST data structures that correspond do the data stored in PDMap and
// ReversePDMap.
void ProgramInfo::enterCompilationUnit(ASTContext &Context) {
  assert(persisted == true);
  // Get a set of all of the PersistentSourceLoc's we need to fill in
  std::set<PersistentSourceLoc> P;
  for (auto I : PersistentVariables)
    P.insert(I.first);

  // Resolve the PersistentSourceLoc to one of Decl,Stmt,Type.
  MappingVisitor V(P, Context);
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  for (const auto &D : TUD->decls())
    V.TraverseDecl(D);
  std::pair<std::map<PersistentSourceLoc, MappingVisitor::StmtDeclOrType>,
    VariableDecltoStmtMap>
    res = V.getResults();
  std::map<PersistentSourceLoc, MappingVisitor::StmtDeclOrType> 
    PSLtoDecl = res.first;

  // Re-populate Variables.
  assert(Variables.empty());
  for (auto I : PersistentVariables) {
    PersistentSourceLoc PL = I.first;
    uint32_t V = I.second;
    std::map<PersistentSourceLoc, MappingVisitor::StmtDeclOrType>::iterator K = 
      PSLtoDecl.find(PL);
    if (K != PSLtoDecl.end()) {
      Decl *D;
      Stmt *S;
      Type *T;
      std::tie<Stmt *, Decl *, Type *>(S, D, T) = K->second;
      assert(D != NULL);
      Variables[D] = V;
    }
  }

  // Re-populate RVariables.
  assert(RVariables.empty());
  for (auto I : PersistentRVariables) {
    PersistentSourceLoc PL = I.second;
    uint32_t V = I.first;
    std::map<PersistentSourceLoc, MappingVisitor::StmtDeclOrType>::iterator K = 
      PSLtoDecl.find(PL);
    if (K != PSLtoDecl.end()) {
      Decl *D;
      Stmt *S;
      Type *T;
      std::tie<Stmt *, Decl *, Type *>(S, D, T) = K->second;
      assert(D != NULL);
      RVariables[V] = D;
    }
  }

  // Re-populate DepthMap.
  assert(DepthMap.empty());
  for (auto I : PersistentDepthMap) {
    PersistentSourceLoc PL = I.first;
    uint32_t V = I.second;
    std::map<PersistentSourceLoc, MappingVisitor::StmtDeclOrType>::iterator K = 
      PSLtoDecl.find(PL);
    if (K != PSLtoDecl.end()) {
      Decl *D;
      Stmt *S;
      Type *T;
      std::tie<Stmt *, Decl *, Type *>(S, D, T) = K->second;
      assert(D != NULL);
      DepthMap[D] = V;
    }
  }

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
  Variables.clear();
  VarDeclToStatement.clear();
  RVariables.clear();
  DepthMap.clear();
  persisted = true;
  return;
}

// For each pointer type in the declaration of D, add a variable to the
// constraint system for that pointer type.
bool ProgramInfo::addVariable(Decl *D, DeclStmt *St, ASTContext *C) {
  assert(persisted == false);
  PersistentSourceLoc PLoc = 
    PersistentSourceLoc::mkPSL(D, *C);

  assert(PLoc.valid());

  // Check if we already have this Decl.
  if (Variables.find(D) == Variables.end()) {
    std::map<PersistentSourceLoc, uint32_t>::iterator Itmp = 
      PersistentVariables.find(PLoc);
    // We don't have the Decl in Variables, but we DO have the Decl in the
    // PersistentVariables map. This means that this Decl and other Decls
    // are defined in the same location in the source of the program. 
    // We can treat them as being aliases of each other for now. 
    if (Itmp != PersistentVariables.end()) 
      return true;
    

    uint32_t thisKey = freeKey;
    Variables.insert(std::pair<Decl *, uint32_t>(D, thisKey));
    PersistentVariables.insert(std::pair<PersistentSourceLoc, uint32_t>
      (PLoc,thisKey));
    
    if (St && VarDeclToStatement.find(D) == VarDeclToStatement.end())
      VarDeclToStatement.insert(std::pair<Decl *, DeclStmt *>(D, St));

    // Get a type to tear apart piece by piece.
    const Type *Ty = NULL;
    if (VarDecl *VD = dyn_cast<VarDecl>(D))
      Ty = VD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    else if (FieldDecl *FD = dyn_cast<FieldDecl>(D))
      Ty = FD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D))
      Ty = UD->getTypeSourceInfo()->getTypeLoc().getTypePtr();
    else
      llvm_unreachable("unknown decl type");

    assert(Ty != NULL);
    Ty = Ty->getUnqualifiedDesugaredType();

    // Strip off function types.
    while (Ty != NULL) {
      if (const FunctionType *FT = dyn_cast<FunctionType>(Ty))
        Ty = FT->getReturnType().getTypePtr()->getUnqualifiedDesugaredType();
      else if (const FunctionNoProtoType *FNPT = dyn_cast<FunctionNoProtoType>(Ty))
        Ty = FNPT->getReturnType().getTypePtr()->getUnqualifiedDesugaredType();
      else
        break;
    }

    while (Ty != NULL) {
      if (Ty->isPointerType()) {
        RVariables.insert(std::pair<uint32_t, Decl *>(thisKey, D));
        PersistentRVariables[thisKey] = PLoc;
        CS.getOrCreateVar(thisKey);

        thisKey++;
        freeKey++;
      } else {
        break;
      }

      Ty = getNextTy(Ty);
    }

    DepthMap.insert(std::pair<Decl *, uint32_t>(D, freeKey));
    PersistentDepthMap[PLoc] = freeKey;

    return true;
  } else {
    assert(PersistentVariables.find(PLoc) != PersistentVariables.end());
    return false;
  }
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

bool
ProgramInfo::declHelper(Decl *D,
  std::set < std::tuple<uint32_t, uint32_t, uint32_t> > &V,
  ASTContext *C) {
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(D, *C);
  assert(PSL.valid());

  VariableMap::iterator I = Variables.find(D);
  std::map<PersistentSourceLoc, uint32_t>::iterator IN =
    PersistentVariables.find(PSL);

  if (I != Variables.end()) {
    assert(IN != PersistentVariables.end());
    DeclMap::iterator DI = DepthMap.find(D);
    assert(DI != DepthMap.end());
    V.insert(std::tuple<uint32_t, uint32_t, uint32_t>
      (I->second, I->second, DI->second));
    return true;
  }
  else {
    // TODO: Also check if IN != end(), if it doesn't, then give back
    //       that variable as well. This is the case where the variable
    //       naming scheme is confused, but all of the confused variables
    //       should alias.
    if (IN != PersistentVariables.end()) {
      uint32_t var = IN->second;
      Decl *dl = getDecl(var);
      DeclMap::iterator DI = DepthMap.find(dl);
      assert(DI != DepthMap.end());
      V.insert(std::tuple<uint32_t, uint32_t, uint32_t>
        (var, var, DI->second));
      return true;
    } else {
      return false;
    }
  }
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
bool 
ProgramInfo::getVariableHelper(Expr *E,
                    std::set<std::tuple<uint32_t, uint32_t, uint32_t> > &V,
                    ASTContext *C) {
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
    return declHelper(DRE->getDecl(), V, C);
  } else if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
    return declHelper(ME->getMemberDecl(), V, C);
  } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
    return getVariableHelper(BO->getLHS(), V, C) ||
           getVariableHelper(BO->getRHS(), V, C);
  } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
    if (getVariableHelper(UO->getSubExpr(), V, C)) {
      if (UO->getOpcode() == UO_Deref) {
        bool b = true;
        std::set< std::tuple<uint32_t, uint32_t, uint32_t> > R;

        for (std::set< std::tuple<uint32_t, uint32_t, uint32_t> >::iterator I =
          V.begin(); I != V.end(); ++I)
        {
          uint32_t curVar, baseVar, limVar;
          std::tie(curVar, baseVar, limVar) = *I;
          uint32_t tmpVar = curVar + 1;
          R.insert(std::tuple<uint32_t, uint32_t, uint32_t>
            (tmpVar, baseVar, limVar));
          b &= (tmpVar >= baseVar && tmpVar < limVar);
        }

        V.swap(R);
        return b;
      } 
      // TODO: Should UO_AddrOf be handled here too?
      return true;
    }
    return false;
  } else if (ImplicitCastExpr *IE = dyn_cast<ImplicitCastExpr>(E)) {
    return getVariableHelper(IE->getSubExpr(), V, C);
  } else if (ParenExpr *PE = dyn_cast<ParenExpr>(E)) {
    return getVariableHelper(PE->getSubExpr(), V, C);
  } else if (CallExpr *CE = dyn_cast<CallExpr>(E)) {
    return getVariableHelper(CE->getCallee(), V, C);
  } else if (ConditionalOperator *CO = dyn_cast<ConditionalOperator>(E)) {
    // Explore the three exprs individually.
    // TODO: Do we need to give these three sub-explorations their own sets
    //       and merge them at this point?
    bool r = false;
    r |= getVariableHelper(CO->getCond(), V, C);
    r |= getVariableHelper(CO->getLHS(), V, C);
    r |= getVariableHelper(CO->getRHS(), V, C);
    return r;
  } else {
    return false;
  }
}

// Given some expression E, what is the top-most constraint variable that
// E refers to? It could be none, in which case V is empty. Otherwise, V 
// contains the constraint variable(s) that E refers to.
void ProgramInfo::getVariable(Expr *E, std::set<uint32_t> &V, ASTContext *C) {
  assert(persisted == false);
  if (!E)
    return;

  std::set<std::tuple<uint32_t, uint32_t, uint32_t> > VandDepth;
  if (getVariableHelper(E, VandDepth, C)) {
    for (auto I : VandDepth) {
      uint32_t var, base, lim;
      std::tie(var, base, lim) = I;
      V.insert(var);
    }
    return;
  }

  return;
}

// Given a decl, return the variables for the constraints of the Decl.
void ProgramInfo::getVariable(Decl *D, std::set<uint32_t> &V, ASTContext *C) {
  assert(persisted == false);
  if (!D)
    return;

  auto I = DepthMap.find(D);
  auto J = Variables.find(D);
  if (I != DepthMap.end() && J != Variables.end()) {
    uint32_t baseVar = J->second;
    uint32_t limVar = I->second;
    for (; baseVar < limVar; baseVar++) 
      V.insert(baseVar);
  }

  return;
}

// Given a constraint variable identifier K, find the Decl that
// corresponds to that variable. Note that multiple constraint
// variables map to a single decl, as in the case of
// int **b; for example. In this case, there would be two variables
// for that Decl, read out like int * q_0 * q_1 b;
// Returns NULL if there is no Decl for that varabiel.
Decl *ProgramInfo::getDecl(uint32_t K) {
  assert(persisted == false);
  auto I = RVariables.find(K);
  if (I == RVariables.end())
    return NULL;

  return I->second;
}
