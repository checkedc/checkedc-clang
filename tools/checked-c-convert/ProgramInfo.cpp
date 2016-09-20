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

static
std::string
tyToStr(const Type *T) {
  QualType QT(T, 0);

  return QT.getAsString();
}

PointerVariableConstraint::PointerVariableConstraint(DeclaratorDecl *D,
  uint32_t &K, Constraints &CS) :
  PointerVariableConstraint(D->getType().getTypePtr(), K, D->getName(), CS) { }

PointerVariableConstraint::PointerVariableConstraint(const Type *_Ty,
  uint32_t &K, std::string N, Constraints &CS) :
  ConstraintVariable(ConstraintVariable::PointerVariable, tyToStr(_Ty)),
  FV(nullptr)
{
  const Type *Ty = nullptr;
  for (Ty = _Ty;
    Ty->isPointerType();
    Ty = getNextTy(Ty))
  {    
    // Allocate a new constraint variable for this level of pointer.
    vars.insert(K);
    CS.getOrCreateVar(K);
    K++;
  }

  // If, after boiling off the pointer-ness from this type, we hit a 
  // function, then create a base-level FVConstraint that we carry 
  // around too.
  if (Ty->isFunctionType()) {
    FV = new FVConstraint(Ty, K, N, CS);
  }

  BaseType = tyToStr(Ty);
  
}

void PointerVariableConstraint::print(raw_ostream &O) const {
  O << "{ ";
  for (const auto &I : vars) 
    O << "q_" << I << " ";
  O << " }";
}

std::string
PointerVariableConstraint::mkString(Constraints::EnvironmentMap &E) {
  std::string s = "";
  unsigned caratsToAdd = 0;
  bool emittedBase = false;
  for (const auto &V : vars) {
    VarAtom VA(V);
    ConstAtom *C = E[&VA];
    assert(C != nullptr);

    switch (C->getKind()) {
    case Atom::A_Ptr:
      emittedBase = false;
      s = s + "_Ptr<";

      caratsToAdd++;
      break;
    case Atom::A_Arr:
    case Atom::A_Wild:
      if (emittedBase) {
        s = s + "*";
      } else {
        assert(BaseType.size() > 0);
        emittedBase = true;
        if (FV)
          s = s + FV->mkString(E);
        else
          s = s + BaseType + "*";
      }
      break;
    case Atom::A_Const:
    case Atom::A_Var:
      llvm_unreachable("impossible");
      break;
    }
  }

  if(emittedBase == false) {
    // If we have a FV pointer, then our "base" type is a function pointer
    // type.
    if (FV)
      s = s + FV->mkString(E);
    else
      s = s + BaseType;
  }

  for (unsigned i = 0; i < caratsToAdd; i++) {
    s = s + ">";
  }

  s = s + " ";

  return s;
}

// This describes a function, either a function pointer or a function
// declaration itself. Either require constraint variables for any pointer
// types that are either return values or paraemeters for the function.
FunctionVariableConstraint::FunctionVariableConstraint(DeclaratorDecl *D,
  uint32_t &K, Constraints &CS) :
  FunctionVariableConstraint(D->getType().getTypePtr(), K, D->getName(), CS) {}

FunctionVariableConstraint::FunctionVariableConstraint(const Type *Ty,
    uint32_t &K, std::string N, Constraints &CS) :
  ConstraintVariable(ConstraintVariable::FunctionVariable, tyToStr(Ty)),name(N)
{
  const Type *returnType = nullptr;
  std::vector<const Type*> paramTypes;
  hasproto = false;
  if (Ty->isFunctionPointerType()) {
    // Is this a function pointer definition?
    llvm_unreachable("should not hit this case");
  } else if (Ty->isFunctionProtoType()) {
    // Is this a function? 
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    assert(FT != nullptr); 
    returnType = FT->getReturnType().getTypePtr();
    for (unsigned i = 0; i < FT->getNumParams(); i++) 
      paramTypes.push_back(FT->getParamType(i).getTypePtr());
    hasproto = true;
  }
  else if (Ty->isFunctionNoProtoType()) {
    const FunctionNoProtoType *FT = Ty->getAs<FunctionNoProtoType>();
    assert(FT != nullptr);
    returnType = FT->getReturnType().getTypePtr();
  } else {
    Ty->dump();
    llvm_unreachable("don't know what to do");
  }

  if (returnType->isPointerType())
    returnVars.insert(new PVConstraint(returnType, K, N, CS));

  for (const auto &P : paramTypes) {
    std::set<ConstraintVariable*> C;
    if (P->isPointerType()) {
      C.insert(new PVConstraint(P, K, N, CS));
      paramVars.push_back(C);
    } else {
      paramVars.push_back(C);
    }
  }
}

void FunctionVariableConstraint::constrainTo(Constraints &CS, ConstAtom *A) {
  for (const auto &V : returnVars)
    V->constrainTo(CS, A);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainTo(CS, A);
}

bool FunctionVariableConstraint::anyChanges(Constraints::EnvironmentMap &E) {
  bool f = false;

  for (const auto &C : returnVars)
    f |= C->anyChanges(E);

  for (const auto &I : paramVars)
    for (const auto &C : I)
      f |= C->anyChanges(E);

  return f;
}

void PointerVariableConstraint::constrainTo(Constraints &CS, ConstAtom *A) {
  for (const auto &V : vars)
    CS.addConstraint(CS.createEq(CS.getOrCreateVar(V), A));

  if (FV)
    FV->constrainTo(CS, A);
}

bool PointerVariableConstraint::anyChanges(Constraints::EnvironmentMap &E) {
  bool f = false;

  for (const auto &C : vars) {
    VarAtom V(C);
    ConstAtom *CS = E[&V];
    assert(CS != nullptr);
    f |= isa<PtrAtom>(CS);
  }

  if (FV)
    f |= FV->anyChanges(E);

  return f;
}

void FunctionVariableConstraint::print(raw_ostream &O) const {
  O << "( ";
  for(const auto &I : returnVars)
   I->dump(); 
  O << " )";
  O << " " << name << " ";
  for(const auto &I : paramVars) {
    O << "( ";
    for(const auto &J : I)
      J->dump();
    O << " )";
  }
}

std::string
FunctionVariableConstraint::mkString(Constraints::EnvironmentMap &E) {
  assert(name.size() > 0);

  return "";
}

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

// Print out statistics of constraint variables on a per-file basis.
void ProgramInfo::print_stats(std::set<std::string> &F, raw_ostream &O) {
  std::map<std::string, std::tuple<int, int, int, int> > filesToVars;
  Constraints::EnvironmentMap env = CS.getVariables();

  // First, build the map and perform the aggregation.
  /*for (const auto &I : PersistentRVariables) {
    const std::string fileName = I.second.getFileName();
    if (F.count(fileName)) {
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
  }*/

  // Then, dump the map to output.

  O << "file|#constraints|#ptr|#arr|#wild\n";
  for (const auto &I : filesToVars) {
    int v, p, a, w;
    std::tie(v, p, a, w) = I.second;
    O << I.first << "|" << v << "|" << p << "|" << a << "|" << w;
    O << "\n";
  }
}

bool ProgramInfo::checkStructuralEquality(std::set<ConstraintVariable*> V, 
                                          std::set<ConstraintVariable*> U) {
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
    /*std::set<GlobalFunctionSymbol*> funcs;
    std::set<GlobalVariableSymbol*> vars;
    for (const auto &U : S.second)
      if (GlobalFunctionSymbol *K = dyn_cast<GlobalFunctionSymbol>(U))
        funcs.insert(K);
      else if (GlobalVariableSymbol *V = dyn_cast<GlobalVariableSymbol>(U))
        vars.insert(V);*/

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
    /*for (std::set<GlobalFunctionSymbol*>::iterator I = funcs.begin();
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
    }*/

    // Do the same as above, but in a simpler case where we only need to 
    // constrain according to the type of the global variable. 
    //for (std::set<GlobalVariableSymbol*>::iterator I = vars.begin();
    //  I != vars.end(); ++I) {

    //}
  }

  // For every global function that is an unresolved external, constrain 
  // its parameter types to be wild.
  for (const auto &U : ExternFunctions) {
    // If we've seen this symbol, but never seen a body for it, constrain
    // everything about it.
    if (U.second == false) {
      std::string UnkSymbol = U.first;
      std::map<std::string, std::set<FVConstraint*> >::iterator I =
        GlobalSymbols.find(UnkSymbol);
      assert(I != GlobalSymbols.end());
      const std::set<FVConstraint*> &Gs = (*I).second;

      for (const auto &G : Gs) {
        for(const auto &U : G->getReturnVars()) 
          U->constrainTo(CS, CS.getWild());

        for(unsigned i = 0; i < G->numParams(); i++) 
          for(const auto &U : G->getParamVar(i)) 
            U->constrainTo(CS, CS.getWild());
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
  std::pair<std::map<PersistentSourceLoc, MappingVisitor::StmtDeclOrType>,
    VariableDecltoStmtMap>
    res = V.getResults();
  std::map<PersistentSourceLoc, MappingVisitor::StmtDeclOrType> 
    PSLtoDecl = res.first;

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
  
  if (Ty->isPointerType()) 
    // Create a pointer value for the type.
    P = new PVConstraint(D, freeKey, CS);

  // Only create a function type if the type is a base Function type. The case
  // for creating function pointers is handled above, with a PVConstraint that
  // contains a FVConstraint.
  if (Ty->isFunctionType()) 
    // Create a function value for the type.
    F = new FVConstraint(D, freeKey, CS);

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
std::set<ConstraintVariable *> 
ProgramInfo::getVariableHelper(Expr *E, 
  std::set<ConstraintVariable *> V, ASTContext *C) {
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E)) {
    return getVariable(DRE->getDecl(), C);
  } else if (MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
    return getVariable(ME->getMemberDecl(), C);
  } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
    std::set<ConstraintVariable*> T1 = getVariableHelper(BO->getLHS(), V, C);
    std::set<ConstraintVariable*> T2 = getVariableHelper(BO->getRHS(), V, C);
    T1.insert(T2.begin(), T2.end());
    return T1;
  } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
    std::set<ConstraintVariable *> T = 
      getVariableHelper(UO->getSubExpr(), V, C);
   
    std::set<ConstraintVariable*> updt;
    std::set<ConstraintVariable*> tmp;
    if (UO->getOpcode() == UO_Deref) {
      for (const auto &CV : T) {
        if (PVConstraint *PVC = dyn_cast<PVConstraint>(CV)) {
          // Subtract one from this constraint. If that generates an empty 
          // constraint, then, don't add it 
          std::set<uint32_t> C = PVC->getCvars();
          assert(C.size() > 0);
          C.erase(C.begin());
          if (C.size() > 0)
            tmp.insert(new PVConstraint(C, PVC->getTy()));
        } else {
          llvm_unreachable("Shouldn't dereference a function pointer!");
        }
      }
      T.swap(tmp);
    }

    return T;
  } else if (ImplicitCastExpr *IE = dyn_cast<ImplicitCastExpr>(E)) {
    return getVariableHelper(IE->getSubExpr(), V, C);
  } else if (ParenExpr *PE = dyn_cast<ParenExpr>(E)) {
    return getVariableHelper(PE->getSubExpr(), V, C);
  } else if (CallExpr *CE = dyn_cast<CallExpr>(E)) {
    // Here, we need to look up the target of the call and return the
    // constraints for the return value of that function.
    Decl *D = CE->getCalleeDecl();
    assert(D != nullptr);
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      std::set<ConstraintVariable*> CS = getVariable(FD, C);
      std::set<ConstraintVariable*> TR;
      FVConstraint *FVC = nullptr;
      for (const auto &J : CS)
        if (FVConstraint *tmp = dyn_cast<FVConstraint>(J))
          FVC = tmp;
      assert(FVC != nullptr); // Should have found a FVConstraint
      TR.insert(FVC->getReturnVars().begin(), FVC->getReturnVars().end());
      return TR;
    } else {
      llvm_unreachable("TODO");
    }
    //return getVariableHelper(CE->getCallee(), V, C);
  } else if (ConditionalOperator *CO = dyn_cast<ConditionalOperator>(E)) {
    // Explore the three exprs individually.
    std::set<ConstraintVariable*> T;
    std::set<ConstraintVariable*> R;
    T = getVariableHelper(CO->getCond(), V, C);
    R.insert(T.begin(), T.end());
    T = getVariableHelper(CO->getLHS(), V, C);
    R.insert(T.begin(), T.end());
    T = getVariableHelper(CO->getRHS(), V, C);
    R.insert(T.begin(), T.end());
    return R;
  } else {
    return std::set<ConstraintVariable*>();
  }
}

// Given a decl, return the variables for the constraints of the Decl.
std::set<ConstraintVariable*>
ProgramInfo::getVariable(Decl *D, ASTContext *C) {
  assert(persisted == false);
  VariableMap::iterator I = Variables.find(PersistentSourceLoc::mkPSL(D, *C));
  if (I != Variables.end()) 
    return I->second;
   else 
    return std::set<ConstraintVariable*>();
}
// Given some expression E, what is the top-most constraint variable that
// E refers to? It could be none, in which case the returned set is empty. 
// Otherwise, the returned setcontains the constraint variable(s) that E 
// refers to.
std::set<ConstraintVariable*>
ProgramInfo::getVariable(Expr *E, ASTContext *C) {
  assert(persisted == false);

  // Get the constraint variables represented by this Expr
  std::set<ConstraintVariable*> T;
  if (E)
    return getVariableHelper(E, T, C);
  else
    return T;
}
