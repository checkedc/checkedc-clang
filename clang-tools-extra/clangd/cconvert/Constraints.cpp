//=--Constraints.cpp----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Constraint solver implementation
//
//===----------------------------------------------------------------------===//

#include <set>
#include "llvm/Support/CommandLine.h"

#include "Constraints.h"
#include "PersistentSourceLoc.h"
#include "Utils.h"
#include "CCGlobalOptions.h"

using namespace llvm;

static cl::OptionCategory SolverCategory("solver options");
static cl::opt<bool> DebugSolver("debug-solver",
  cl::desc("Dump intermediate solver state"),
  cl::init(false), cl::cat(SolverCategory));

unsigned
VarAtom::replaceEqConstraints(Constraints::EnvironmentMap &VAtoms,
                              class Constraints &CS) {
  unsigned NumRemConstraints = 0;
  std::set<Constraint *, PComp<Constraint *>> ConstraintsToRem;
  ConstraintsToRem.clear();
  std::set<Constraint *, PComp<Constraint *>> OldConstraints;
  OldConstraints.clear();
  OldConstraints.insert(Constraints.begin(), Constraints.end());

  for (auto CC : OldConstraints) {
    for (auto &VatomP : VAtoms) {
      ConstAtom *CCons = VatomP.second;
      VarAtom *DVatom = VatomP.first;
      // Check if the constraint contains
      // the provided constraint variable.
      if (CC->containsConstraint(DVatom) && dyn_cast<Eq>(CC)) {
        NumRemConstraints++;
        // This has to be an equality constraint.
        Eq *EqCons = dyn_cast<Eq>(CC);
        // We will modify this constraint remove it
        // from the local and global sets.
        CS.removeConstraint(CC);
        Constraints.erase(CC);

        // Mark this constraint to be deleted.
        ConstraintsToRem.insert(CC);

        assert(EqCons != nullptr &&
               "Do not know how to replace a non-equality constraint.");
        if (CCons != nullptr) {
          Eq *NewC = nullptr;
          if (*(EqCons->getLHS()) == *(DVatom)) {
            // If this is of the form var1 = var2.
            if (dyn_cast<VarAtom>(EqCons->rhs)) {
              // Create a constraint var2 = const.
              VarAtom *VA = dyn_cast<VarAtom>(EqCons->rhs);
              NewC = CS.createEq(VA, CCons);
            } else {
              // Else, create a constraint var1 = const.
              VarAtom *VA = dyn_cast<VarAtom>(EqCons->lhs);
              NewC = CS.createEq(VA, CCons);
            }
          }
          // If we have created a new equality constraint.
          if (NewC) {
            // Add the constraint.
            if (!CS.addConstraint(NewC)) {
              // If this is already added?
              // delete it.
              delete(NewC);
            }
          }
        }
      }
    }
  }

  for (auto ToDel : ConstraintsToRem) {
    delete(ToDel);
  }

  return NumRemConstraints;
}

Constraint::Constraint(ConstraintKind K, std::string &Rsn,
                       PersistentSourceLoc *PL): Constraint(K, Rsn) {
  if (PL != nullptr && PL->valid()) {
    FileName = PL->getFileName();
    LineNo = PL->getLineNo();
    ColStart = PL->getColNo();
  }
}

// Remove the constraint from the global constraint set.
bool Constraints::removeConstraint(Constraint *C) {
  removeReasonBasedConstraint(C);
  return constraints.erase(C) != 0;
}

// Check if we can add this constraint. This provides a global switch to
// control what constraints we can add to our system.
void Constraints::editConstraintHook(Constraint *C) {
  if (!AllTypes) {
    // If this is an equality constraint, check if we are adding
    // only Ptr or WILD constraints? if not? make it WILD.
    if (Eq *E = dyn_cast<Eq>(C)) {
      if (ConstAtom *RConst = dyn_cast<ConstAtom>(E->getRHS())) {
        if (!(isa<PtrAtom>(RConst) || isa<WildAtom>(RConst))) {
          // Can we assign WILD to the left side var?.
          VarAtom *LHSA = dyn_cast<VarAtom>(E->getLHS());
          if (!LHSA || LHSA->canAssign(getWild()))
            E->setRHS(getWild());
        }
      }
    }
  }
}

// Add a constraint to the set of constraints. If the constraint is already 
// present (by syntactic equality) return false. 
bool Constraints::addConstraint(Constraint *C) {
  // Validate the constraint to be added.
  assert(check(C));

  editConstraintHook(C);

  // Check if C is already in the set of constraints. 
  if (constraints.find(C) == constraints.end()) {
    constraints.insert(C);
    addReasonBasedConstraint(C);

    // Update the variables that depend on this constraint.
    if (Eq *E = dyn_cast<Eq>(C)) {
      if (VarAtom *vLHS = dyn_cast<VarAtom>(E->getLHS()))
        vLHS->Constraints.insert(C);
    }
    else if (Not *N = dyn_cast<Not>(C)) {
      if (Eq *E = dyn_cast<Eq>(N->getBody())) {
        if (VarAtom *vLHS = dyn_cast<VarAtom>(E->getLHS()))
          vLHS->Constraints.insert(C);

      }
    }
    else if (Implies *I = dyn_cast<Implies>(C)) {
      if (Eq *E = dyn_cast<Eq>(I->getPremise())) {
        if (VarAtom *vLHS = dyn_cast<VarAtom>(E->getLHS()))
          vLHS->Constraints.insert(C);
      }
    }
    else
      llvm_unreachable("unsupported constraint");
    return true;
  }

  return false;
}

bool Constraints::addReasonBasedConstraint(Constraint *C) {
  // Only insert if this is an Eq constraint and has a valid reason.
  if (Eq *E = dyn_cast<Eq>(C)) {
    if (E->getReason() != DEFAULT_REASON && !E->getReason().empty())
      return this->constraintsByReason[E->getReason()].insert(E).second;
  }
  return false;
}

bool Constraints::removeReasonBasedConstraint(Constraint *C) {
  if (Eq *E = dyn_cast<Eq>(C)) {
    // Remove if the constraint is present.
    if (this->constraintsByReason.find(E->getReason()) !=
        this->constraintsByReason.end())
      return this->constraintsByReason[E->getReason()].erase(E) > 0;
  }
  return false;
}

// Checks to see if the constraint is of a form that we expect.
// The expected forms are the following:
// EQ : (q_i = A) | (q_i = q_k) for A constant or
// NOT : NOT(q_i = A) for A constant or
// IMPLIES : (q_i = A) => (q_k = B) for A,B constant
bool Constraints::check(Constraint *C) {

  if (Not *N = dyn_cast<Not>(C)) {
    if (Eq *E = dyn_cast<Eq>(N->getBody()))
      if (!isa<VarAtom>(E->getLHS()) || isa<VarAtom>(E->getRHS()))
        return false;
  }
  else if (Implies *I = dyn_cast<Implies>(C)) {
    if (Eq *P = dyn_cast<Eq>(I->getPremise())) {
      if (!isa<VarAtom>(P->getLHS()) || isa<VarAtom>(P->getRHS()))
        return false;
    }
    else {
      return false;
    }

    if (Eq *CO = dyn_cast<Eq>(I->getConclusion())) {
      if (!isa<VarAtom>(CO->getLHS()) || isa<VarAtom>(CO->getRHS()))
        return false;
    }
    else {
      return false;
    }
  }
  else if (Eq *E = dyn_cast<Eq>(C)) {

    if (!isa<VarAtom>(E->getLHS()))
      return false;
  }

  return true;
}

// Function that handles assignment of the provided ConstAtom to
// the provided srcVar.
// Returns true if the assignment has been made.
bool Constraints::assignConstToVar(EnvironmentMap::iterator &SrcVar,
                                   ConstAtom *C) {
  if (SrcVar->first->canAssign(C)) {
    SrcVar->second = C;
    return true;
  }
  return false;
}

// Given an equality constraint _Dyn_, and a current variable binding 
// _CurValLHS_, where _CurValLHS_ represents the pair (q_i:C) and the
// equality constraint _Dyn_ := q_i == K, pattern match over K. It 
// could be either a constant value such as WildAtom, or, it could be
// another variable. 
//
// T is constrained to be one of the types from the constant lattice. 
// T is parametric because the logic for equality propagation is common
// between different cases of constraint solving. 
// 
// Return true if propEq modified the binding of (q_i:C) or the binding
// of (q_j:K) if _Dyn_ was of the form q_i == q_k.
template <typename T>
bool
Constraints::propEq(EnvironmentMap &E, Eq *Dyn, T *A, ConstraintSet &R,
  EnvironmentMap::iterator &CurValLHS) {
  bool ChangedEnv = false;

  if (isa<T>(Dyn->getRHS())) {
    if (*(CurValLHS->second) < *A) {
      R.insert(Dyn);
      ChangedEnv = assignConstToVar(CurValLHS, A);
    }
  } // Also propagate from equality when v = v'.
  else if (VarAtom *RHSVar = dyn_cast<VarAtom>(Dyn->getRHS())) {
    EnvironmentMap::iterator CurValRHS = E.find(RHSVar);
    assert(CurValRHS != E.end()); // The var on the RHS should be in the env.

    if (*(CurValLHS->second) < *(CurValRHS->second)) {
      ChangedEnv = assignConstToVar(CurValLHS, CurValRHS->second);
    }
    else if (*(CurValRHS->second) < *(CurValLHS->second)) {
      ChangedEnv = assignConstToVar(CurValRHS, CurValLHS->second);;
    }
    else
      assert(*(CurValRHS->second) == *(CurValLHS->second));
  }

  return ChangedEnv;
}

// Propagates implication through the environment for a single 
// variable (whose value is given by _V_) used in an implication 
// constraint _Imp_.
template <typename T>
bool
Constraints::propImp(Implies *Imp, T *A, ConstraintSet &R, ConstAtom *V) {
  Constraint *Con = nullptr;
  bool ChangedEnv = false;

  if (Eq *DynP = dyn_cast<Eq>(Imp->getPremise())) 
    if (isa<T>(DynP->getRHS()) && *V == *A) {
      Con = Imp->getConclusion();
      R.insert(Imp);
      addConstraint(Con);
      ChangedEnv = true;
    }

  return ChangedEnv;
}

// This method checks if the template
// const atom can be assigned to the provided (src)
// variable.
template <typename T>
bool Constraints::canAssignConst(VarAtom *Src) {

  for (const auto &C : Src->Constraints) {
    // Check if there is a non-equality constraint
    // of the provided type.
    if (Not *N = dyn_cast<Not>(C)) {
      if (Eq *E = dyn_cast<Eq>(N->getBody())) {
        if (dyn_cast<T>(E->getRHS())) {
          return false;
        }
      }
    }
  }
  return true;
}

// Takes one iteration to solve the system of constraints. Each step 
// involves the propagation of quantifiers and the potential firing of
// implications. Accepts a single parameter, _env_, that is a map of 
// variables to their current value in the ConstAtom lattice. 
//
// Returns true if the step didn't change any bindings of variables in
// the environment. 
bool Constraints::step_solve(EnvironmentMap &Env) {
  bool ChangedEnv = false;

  EnvironmentMap::iterator VI = Env.begin();
  // Step 1. Propagate any WILD constraint as far as we can.
  while (VI != Env.end()) {
    // Iterate over the environment, VI is a pair of a variable q_i and 
    // the constant (one of Ptr, Arr, Wild) that the variable is bound to.
    VarAtom *Var = VI->first;
    ConstAtom *Val = VI->second;
    
    ConstraintSet rmConstraints;
    for (const auto &C : Var->Constraints) 
      if (Eq *E = dyn_cast<Eq>(C))
        ChangedEnv |= propEq<WildAtom>(Env, E, getWild(),
                                               rmConstraints, VI);
      else if (Implies *Imp = dyn_cast<Implies>(C))
        ChangedEnv |= propImp<WildAtom>(Imp, getWild(),
                                                rmConstraints, Val);

    for (const auto &RC : rmConstraints)
      Var->eraseConstraint(RC);

    ++VI;
  }

  VI = Env.begin();
  // Step 2. Propagate any ARITH constraints.
  while (VI != Env.end()) {
    VarAtom *Var = VI->first;

    ConstraintSet RemCons;
    for (const auto &C : Var->Constraints) {
      // Re-read the assignment as the propagating might have
      // changed this and the constraints will get removed.
      ConstAtom *Val = VI->second;
      // Propagate the Neg constraint.
      if (Not *N = dyn_cast<Not>(C)) {
        if (Eq *E = dyn_cast<Eq>(N->getBody())) {
          // If this is Not ( q == Ptr )
          if (isa<PtrAtom>(E->getRHS())) {
            if (!AllTypes && *Val < *getWild()) {
              // This pointer cannot be Ptr.
              // And allTypes is disabled, only choice is to make it WILD.
              VI->second = getWild();
              ChangedEnv = true;
              // Check if we can make it an Arr?
            } else if (*Val < *getArr() && canAssignConst<ArrAtom>(Var)) {
              // Yes? make it Arr.
              VI->second = getArr();
              ChangedEnv = true;
            }
          }
        }
      } else if (Eq *E = dyn_cast<Eq>(C)) {
        ChangedEnv |= propEq<NTArrAtom>(Env, E, getNTArr(),
                                                RemCons, VI);
        ChangedEnv |= propEq<ArrAtom>(Env, E, getArr(),
                                              RemCons, VI);
      } else if (Implies *Imp = dyn_cast<Implies>(C)) {
        ChangedEnv |= propImp<NTArrAtom>(Imp, getNTArr(),
                                                 RemCons, Val);
        ChangedEnv |= propImp<ArrAtom>(Imp, getArr(),
                                               RemCons, Val);
      }
    }

    // NTArray adjustment.
    if (Var->couldBeNtArr(VI->second)) {
      ChangedEnv |= addConstraint(createEq(Var, getNTArr()));
    }

    if (Var->getShouldBeArr()) {
      ChangedEnv |= addConstraint(createEq(Var, getArr()));
    }

    if (Var->getShouldBeNtArr()) {
      ChangedEnv |= addConstraint(createEq(Var, getNTArr()));
    }

    for (const auto &RC : RemCons)
      Var->eraseConstraint(RC);

    ++VI;
  }

  return (ChangedEnv == false);
}

std::pair<Constraints::ConstraintSet, bool>
    Constraints::solve(unsigned &NumOfIter) {
  bool Fixed = false;
  Constraints::ConstraintSet Conflicts;

  NumOfIter = 0;
  if (DebugSolver) {
    errs() << "constraints beginning solve\n";
    dump();
  }


  // It's (probably) possible that a pathologically constructed environment 
  // could cause us to loop n**2 times. It would be ideal to have an upper 
  // bound of k*n for k lattice levels and n variables. This will require 
  // some dependency tracking, we will do that later.
  while (Fixed == false) {
    
    if (DebugSolver) {
      errs() << "constraints pre step\n";
      dump();
    }

    Fixed = step_solve(environment);

    if (DebugSolver) {
      errs() << "constraints post step\n";
      dump();
    }

    NumOfIter++;
  }

  return std::pair<Constraints::ConstraintSet, bool>(Conflicts, true);
}

void Constraints::print(raw_ostream &O) const {
  O << "CONSTRAINTS: \n";
  for (const auto &C : constraints) {
    C->print(O);
    O << "\n";
  }

  O << "ENVIRONMENT: \n";
  for (const auto &V : environment) {
    V.first->print(O);
    O << " = ";
    V.second->print(O);
    O << "\n";
  }
}

void Constraints::dump(void) const {
  print(errs());
}

void Constraints::dump_json(llvm::raw_ostream &O) const {
  O << "{\"Constraints\":[";
  bool addComma = false;
  for (const auto &C : constraints) {
    if (addComma) {
      O << ",\n";
    }
    C->dump_json(O);
    addComma = true;
  }
  O << "],\n";

  addComma = false;

  O << "\"Environment\":[";
  for (const auto &V : environment) {
    if (addComma) {
      O << ",\n";
    }
    O << "{\"var\":";
    V.first->dump_json(O);
    O << ", \"value:\":";
    V.second->dump_json(O);
    O << "}";
    addComma = true;
  }
  O << "]}";

}

bool
Constraints::removeAllConstraintsOnReason(std::string &Reason,
                                          ConstraintSet &RemovedCons) {
  // Are there any constraints with this reason?
  bool Removed = false;
  if (this->constraintsByReason.find(Reason) !=
      this->constraintsByReason.end()) {
    RemovedCons.insert(this->constraintsByReason[Reason].begin(),
                  this->constraintsByReason[Reason].end());
    for (auto cToDel : RemovedCons) {
      Removed = this->removeConstraint(cToDel) || Removed;
    }
    return Removed;
  }
  return Removed;
}

VarAtom *Constraints::getOrCreateVar(uint32_t V) {
  VarAtom Tv(V);
  EnvironmentMap::iterator I = environment.find(&Tv);

  if (I != environment.end())
    return I->first;
  else {
    VarAtom *V = new VarAtom(Tv);
    environment[V] = getPtr();
    return V;
  }
}

VarAtom *Constraints::getVar(uint32_t V) const {
  VarAtom Tv(V);
  EnvironmentMap::const_iterator I = environment.find(&Tv);

  if (I != environment.end())
    return I->first;
  else
    return nullptr;
}

PtrAtom *Constraints::getPtr() const {
  return PrebuiltPtr;
}
ArrAtom *Constraints::getArr() const {
  return PrebuiltArr;
}
NTArrAtom *Constraints::getNTArr() const {
  return PrebuiltNTArr;
}
WildAtom *Constraints::getWild() const {
  return PrebuiltWild;
}

ConstAtom *Constraints::getAssignment(uint32_t V) {
  auto CurrVar = getVar(V);
  assert(CurrVar != nullptr && "Queried uncreated constraint variable.");
  return environment[CurrVar];
}

bool Constraints::isWild(uint32_t V) {
  auto CurrVar = getVar(V);
  assert(CurrVar != nullptr && "Queried uncreated constraint variable.");
  return dyn_cast<WildAtom>(environment[CurrVar]) != nullptr;
}

Eq *Constraints::createEq(Atom *Lhs, Atom *Rhs) {
  return new Eq(Lhs, Rhs);
}

Eq *Constraints::createEq(Atom *Lhs, Atom *Rhs, std::string &Rsn) {
  return new Eq(Lhs, Rhs, Rsn);
}

Eq *Constraints::createEq(Atom *Lhs, Atom *Rhs, std::string &Rsn,
                          PersistentSourceLoc *PL) {
  if (PL != nullptr && PL->valid()) {
    // Make this invalid, if the source location is not absolute path
    // this is to avoid crashes in clangd.
    if (PL->getFileName().c_str()[0] != '/')
      PL = nullptr;
  }
  return new Eq(Lhs, Rhs, Rsn, PL);
}

Not *Constraints::createNot(Constraint *Body) {
  return new Not(Body);
}

Implies *Constraints::createImplies(Constraint *Premise,
                                    Constraint *Conclusion) {
  return new Implies(Premise, Conclusion);
}

void Constraints::resetConstraints() {
  // Update all constraints to pointers.
  for (auto &CurrE : environment) {
    CurrE.second = getPtr();
  }
}

bool Constraints::checkInitialEnvSanity() {
  // All variables should be Ptrs.
  for (const auto &EnvVar : environment) {
    if (EnvVar.second != getPtr()) {
      return false;
    }
  }
  return true;
}

Constraints::Constraints() {
  PrebuiltPtr = new PtrAtom();
  PrebuiltArr = new ArrAtom();
  PrebuiltNTArr = new NTArrAtom();
  PrebuiltWild = new WildAtom();
}

Constraints::~Constraints() {
  delete PrebuiltPtr;
  delete PrebuiltArr;
  delete PrebuiltNTArr;
  delete PrebuiltWild;
}
