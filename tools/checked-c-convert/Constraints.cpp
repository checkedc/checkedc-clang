//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "Constraints.h"
#include "llvm/Support/CommandLine.h"
#include <set>

using namespace llvm;

static cl::OptionCategory SolverCategory("solver options");
static cl::opt<bool> DebugSolver("debug-solver",
  cl::desc("Dump intermediate solver state"),
  cl::init(false), cl::cat(SolverCategory));

// Add a constraint to the set of constraints. If the constraint is already 
// present (by syntactic equality) return false. 
bool Constraints::addConstraint(Constraint *C) {
  // Validate the constraint to be added.
  assert(check(C));

  // Check if C is already in the set of constraints. 
  if (constraints.find(C) == constraints.end()) {
    constraints.insert(C);

    // Update the variables that depend on this constraint
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

// Checks to see if the constraint is of a form that we expect.
// The expected forms are the following:
// EQ : (q_i = A) | (q_i = q_k) for A constant or
// NOT : NOT(q_i = A) for A constant or
// IMPLIES : (q_i = A) => (q_k = B) for A,B constant
bool Constraints::check(Constraint *C) {

  if (Not *N = dyn_cast<Not>(C)) {
    if(Eq *E = dyn_cast<Eq>(N->getBody()))
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
Constraints::propEq(EnvironmentMap &env, Eq *Dyn, T *A, ConstraintSet &R,
  EnvironmentMap::iterator &CurValLHS) {
  bool changedEnvironment = false;

  if (isa<T>(Dyn->getRHS())) {
    if (*(CurValLHS->second) < *A) {
      CurValLHS->second = A;
      R.insert(Dyn);
      changedEnvironment = true;
    }
  } // Also propagate from equality when v = v'.
  else if (VarAtom *RHSVar = dyn_cast<VarAtom>(Dyn->getRHS())) {
    EnvironmentMap::iterator CurValRHS = env.find(RHSVar);
    assert(CurValRHS != env.end()); // The var on the RHS should be in the env.

    if (*(CurValLHS->second) < *(CurValRHS->second)) {
      CurValLHS->second = CurValRHS->second;
      changedEnvironment = true;
    }
    else if (*(CurValRHS->second) < *(CurValLHS->second)) {
      CurValRHS->second = CurValLHS->second;
      changedEnvironment = true;
    }
    else
      assert(*(CurValRHS->second) == *(CurValLHS->second));
  }

  return changedEnvironment;
}

// Propagates implication through the environment for a single 
// variable (whose value is given by _V_) used in an implication 
// constraint _Imp_.
template <typename T>
bool
Constraints::propImp(Implies *Imp, T *A, ConstraintSet &R, ConstAtom *V) {
  Constraint *Con = NULL;
  bool changedEnvironment = false;

  if (Eq *DynP = dyn_cast<Eq>(Imp->getPremise())) 
    if (isa<T>(DynP->getRHS()) && *V == *A) {
      Con = Imp->getConclusion();
      R.insert(Imp);
      addConstraint(Con);
      changedEnvironment = true;
    }

  return changedEnvironment;
}

// This method checks if the template
// const atom can be assigned to the provided (src)
// variable.
template <typename T>
bool Constraints::canAssignConst(VarAtom *src) {

  for (const auto &C : src->Constraints) {
    // check if there is a non-equality constraint
    // of the provided type.
    if (Not *N = dyn_cast<Not>(C)) {
      if (Eq *E = dyn_cast<Eq>(N->getBody())) {
        if(dyn_cast<T>(E->getRHS())) {
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
bool Constraints::step_solve(EnvironmentMap &env) {
  bool changedEnvironment = false;

  EnvironmentMap::iterator VI = env.begin();
  // Step 1. Propagate any WILD constraint as far as we can.
  while(VI != env.end()) {
    // Iterate over the environment, VI is a pair of a variable q_i and 
    // the constant (one of Ptr, Arr, Wild) that the variable is bound to.
    VarAtom *Var = VI->first;
    ConstAtom *Val = VI->second;
    
    ConstraintSet rmConstraints;
    for (const auto &C : Var->Constraints) 
      if (Eq *E = dyn_cast<Eq>(C)) 
        changedEnvironment |= propEq<WildAtom>(env, E, getWild(), rmConstraints, VI);
      else if (Implies *Imp = dyn_cast<Implies>(C)) 
        changedEnvironment |= propImp<WildAtom>(Imp, getWild(), rmConstraints, Val);

    for (const auto &RC : rmConstraints)
      Var->Constraints.erase(RC);

    ++VI;
  }

  VI = env.begin();
  // Step 2. Propagate any ARITH constraints.
  while(VI != env.end()) {
    VarAtom *Var = VI->first;
    ConstAtom *Val = VI->second;

    ConstraintSet rmConstraints;
    for (const auto &C : Var->Constraints) {
      // Propagate the Neg constraint.
      if (Not *N = dyn_cast<Not>(C)) {
        if (Eq *E = dyn_cast<Eq>(N->getBody())) {
          // If this is Not ( q == Ptr ) or Not ( q == NTArr)
          // and the current value
          // of q is Ptr ( < *getArr() ) and ARR can be assigned then bump q up to Arr.
          if (isa<PtrAtom>(E->getRHS()) || isa<NTArrAtom>(E->getRHS())) {
            if (*Val < *getArr() && canAssignConst<ArrAtom>(Var)) {
              VI->second = getArr();
              changedEnvironment = true;
            }
          }
        }
      }
      else if (Eq *E = dyn_cast<Eq>(C)) {
        changedEnvironment |= propEq<NTArrAtom>(env, E, getNTArr(), rmConstraints, VI);
        changedEnvironment |= propEq<ArrAtom>(env, E, getArr(), rmConstraints, VI);
      } else if (Implies *Imp = dyn_cast<Implies>(C)) {
        changedEnvironment |= propImp<NTArrAtom>(Imp, getNTArr(), rmConstraints, Val);
        changedEnvironment |= propImp<ArrAtom>(Imp, getArr(), rmConstraints, Val);
      }
    }

    for (const auto &RC : rmConstraints)
      Var->Constraints.erase(RC);

    ++VI;
  }

  return (changedEnvironment == false);
}

std::pair<Constraints::ConstraintSet, bool> Constraints::solve(void) {
  bool fixed = false;
  Constraints::ConstraintSet conflicts;

  if (DebugSolver) {
    errs() << "constraints beginning solve\n";
    dump();
  }

  // It's (probably) possible that a pathologically constructed environment 
  // could cause us to loop n**2 times. It would be ideal to have an upper 
  // bound of k*n for k lattice levels and n variables. This will require 
  // some dependency tracking, we will do that later.
  while (fixed == false) {
    
    if (DebugSolver) {
      errs() << "constraints pre step\n";
      dump();
    }

    fixed = step_solve(environment);

    if (DebugSolver) {
      errs() << "constraints post step\n";
      dump();
    }
  }

  return std::pair<Constraints::ConstraintSet, bool>(conflicts, true);
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
    if(addComma) {
      O << ",\n";
    }
    C->dump_json(O);
    addComma = true;
  }
  O << "],\n";

  addComma = false;

  O << "\"Environment\":[";
  for (const auto &V : environment) {
    if(addComma) {
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

VarAtom *Constraints::getOrCreateVar(uint32_t v) {
  VarAtom tv(v);
  EnvironmentMap::iterator I = environment.find(&tv);

  if (I != environment.end())
    return I->first;
  else {
    VarAtom *V = new VarAtom(tv);
    environment[V] = getPtr();
    return V;
  }
}

VarAtom *Constraints::getVar(uint32_t v) const {
  VarAtom tv(v);
  EnvironmentMap::const_iterator I = environment.find(&tv);

  if (I != environment.end())
    return I->first;
  else
    return nullptr;
}

PtrAtom *Constraints::getPtr() const {
  return prebuiltPtr;
}
ArrAtom *Constraints::getArr() const {
  return prebuiltArr;
}
NTArrAtom *Constraints::getNTArr() const {
  return prebuiltNTArr;
}
WildAtom *Constraints::getWild() const {
  return prebuiltWild;
}

Eq *Constraints::createEq(Atom *lhs, Atom *rhs) {
  return new Eq(lhs, rhs);
}

Not *Constraints::createNot(Constraint *body) {
  return new Not(body);
}

Implies *Constraints::createImplies(Constraint *premise, Constraint *conclusion) {
  return new Implies(premise, conclusion);
}

Constraints::Constraints() {
  prebuiltPtr = new PtrAtom();
  prebuiltArr = new ArrAtom();
  prebuiltNTArr = new NTArrAtom();
  prebuiltWild = new WildAtom();
}

Constraints::~Constraints() {
  delete prebuiltPtr;
  delete prebuiltArr;
  delete prebuiltNTArr;
  delete prebuiltWild;
}
