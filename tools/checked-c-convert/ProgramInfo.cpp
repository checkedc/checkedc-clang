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

void ProgramInfo::dump() {
  CS.dump();
  errs() << "\n";

  errs() << "Variables\n";
  for (const auto &I : Variables) {
    I.first->dump();
    errs() << " ==> \n";
    errs() << I.second << "\n";
  }

  return;
}

bool ProgramInfo::checkStructuralEquality(uint32_t V, uint32_t U) {
  // TODO: implement structural equality checking.
  return false;
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
      if (D == NULL) {
        PL.dump();
        errs() << "\n";
      }
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
    PersistentSourceLoc::mkPSL(D->getLocation(), *C);

  assert(PLoc.valid());

  // Check if we already have this Decl.
  if (Variables.find(D) == Variables.end()) {
    std::map<PersistentSourceLoc, uint32_t>::iterator Itmp = 
      PersistentVariables.find(PLoc);
    if (Itmp != PersistentVariables.end()) {
      D->dump();
      PersistentSourceLoc PSLtmp = Itmp->first;
      PSLtmp.dump();
      errs() << "\n";
      PLoc.dump();
      errs() << "\n";
    }

    assert(PersistentVariables.find(PLoc) == PersistentVariables.end());

    uint32_t thisKey = freeKey;
    Variables.insert(std::pair<Decl *, uint32_t>(D, thisKey));
    PersistentVariables[PLoc] = thisKey;

    if (St && VarDeclToStatement.find(D) == VarDeclToStatement.end())
      VarDeclToStatement.insert(std::pair<Decl *, DeclStmt *>(D, St));

    // Get a type to tear apart piece by piece.
    TypeLoc TL;
    if (VarDecl *VD = dyn_cast<VarDecl>(D))
      TL = VD->getTypeSourceInfo()->getTypeLoc();
    else if (ParmVarDecl *PD = dyn_cast<ParmVarDecl>(D))
      TL = VD->getTypeSourceInfo()->getTypeLoc();
    else if (FieldDecl *FD = dyn_cast<FieldDecl>(D))
      TL = FD->getTypeSourceInfo()->getTypeLoc();
    else if (FunctionDecl *UD = dyn_cast<FunctionDecl>(D))
      TL = UD->getTypeSourceInfo()->getTypeLoc();
    else
      llvm_unreachable("unknown decl type");

    assert(!TL.isNull());

    while (!TL.isNull()) {
      if (TL.getTypePtr()->isPointerType()) {
        RVariables.insert(std::pair<uint32_t, Decl *>(thisKey, D));
        PersistentRVariables[thisKey] = PLoc;
        CS.getOrCreateVar(thisKey);

        thisKey++;
        freeKey++;
      }

      TL = TL.getNextTypeLoc();
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
    Decl *D = DRE->getDecl();
    PersistentSourceLoc PSL = 
      PersistentSourceLoc::mkPSL(D->getLocation(), *C);
    assert(PSL.valid());

    VariableMap::iterator I = Variables.find(D);
    std::map<PersistentSourceLoc, uint32_t>::iterator IN = 
      PersistentVariables.find(PSL);

    if (I != Variables.end()) {
      assert(IN != PersistentVariables.end());
      DeclMap::iterator DI = DepthMap.find(D);
      assert(DI != DepthMap.end());
      V.insert(std::tuple<uint32_t,uint32_t,uint32_t>
        (I->second, I->second, DI->second));
      return true;
    } else {
      return false;
    }
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
  } else if(ConditionalOperator *CO = dyn_cast<ConditionalOperator>(E)) {
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

// Given a decl, return the variable for the top-most constraint of that decl.
// Unlike the case for expressions above, this can only ever return a single
// variable.
void ProgramInfo::getVariable(Decl *D, std::set<uint32_t> &V, ASTContext *C) {
  assert(persisted == false);
  if (!D)
    return;

  VariableMap::iterator I = Variables.find(D);
  if (I != Variables.end()) {
    V.insert(I->second);
    return;
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
