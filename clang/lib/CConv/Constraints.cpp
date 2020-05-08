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

#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/Constraints.h"
#include "clang/CConv/ConstraintsGraph.h"
#include <iostream>

using namespace llvm;

static cl::OptionCategory SolverCategory("solver options");
static cl::opt<bool> DebugSolver("debug-solver",
  cl::desc("Dump intermediate solver state"),
  cl::init(false), cl::cat(SolverCategory));
static cl::opt<bool> UseOldSolver("old-solver",
                                 cl::desc("Use legacy solver (deprecated)"),
                                 cl::init(false), cl::cat(SolverCategory));

unsigned
VarAtom::replaceEqConstraints(EnvironmentMap &VAtoms,
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
      if (CC->containsConstraint(DVatom) && (dyn_cast<Eq>(CC) || dyn_cast<Geq>(CC))) {
        NumRemConstraints++;
        // We will modify this constraint remove it
        // from the local and global sets.
        CS.removeConstraint(CC);
        Constraints.erase(CC);

        // Mark this constraint to be deleted.
        ConstraintsToRem.insert(CC);

        if (CCons != nullptr) {
          Constraint *NewC = nullptr;
          Atom *LHS = nullptr;
          Atom *RHS = nullptr;

          // This has to be an (G)equality constraint.
          if (Eq *EqCons = dyn_cast<Eq>(CC)) {
              LHS = EqCons->getLHS();
              RHS = EqCons->getRHS();
          } else if (Geq *GeqCons = dyn_cast<Geq>(CC)) {
              LHS = GeqCons->getLHS();
              RHS = GeqCons->getRHS();
          }

          if (*LHS == *(DVatom)) {
            // If this is of the form var1 = var2.
            if (dyn_cast<VarAtom>(RHS)) {
              // Create a constraint var2 = const.
              VarAtom *VA = dyn_cast<VarAtom>(RHS);
              NewC = CS.createEq(VA, CCons);
            } else {
              // Else, create a constraint var1 = const.
              VarAtom *VA = dyn_cast<VarAtom>(LHS);
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
    if (Geq *E = dyn_cast<Geq>(C)) {
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
    else if (Geq *E = dyn_cast<Geq>(C)) {
      if (VarAtom *vLHS = dyn_cast<VarAtom>(E->getLHS()))
        vLHS->Constraints.insert(C);
      else if (VarAtom *vRHS = dyn_cast<VarAtom>(E->getRHS())) {
        vRHS->Constraints.insert(C);
      }
    }
    else if (Implies *I = dyn_cast<Implies>(C)) {
      Geq *E = I->getPremise();
      if (VarAtom *vLHS = dyn_cast<VarAtom>(E->getLHS()))
        vLHS->Constraints.insert(C);
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
  else if (Geq *E = dyn_cast<Geq>(C)) {
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
  else if (Geq *E = dyn_cast<Geq>(C)) {
      // Remove if the constraint is present.
      if (this->constraintsByReason.find(E->getReason()) !=
          this->constraintsByReason.end())
          return this->constraintsByReason[E->getReason()].erase(E) > 0;
  }
  return false;
}

// Checks to see if the constraint is of a form that we expect.
// The expected forms are the following:
// EQ : (q_i = q_k)
// GEQ : (q_i >= A) for A constant
// IMPLIES : (q_i >= A) => (q_k >= B) for A,B constant
bool Constraints::check(Constraint *C) {

  if (Implies *I = dyn_cast<Implies>(C)) {
    Geq *P = I->getPremise();
    Geq *CO = I->getConclusion();
    if (!isa<VarAtom>(P->getLHS()) || isa<VarAtom>(P->getRHS()) ||
        !isa<VarAtom>(CO->getLHS()) || isa<VarAtom>(CO->getRHS()))
      return false;
  }
  else if (Eq *E = dyn_cast<Eq>(C)) {
    if (!isa<VarAtom>(E->getLHS()) || !isa<VarAtom>(E->getRHS()))
      return false;
  }
  else if (dyn_cast<Geq>(C) != nullptr) {
      // all good!
  }
  else
    return false; // Not Eq, Geq, or Implies; what is it?!

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
// equality constraint _Dyn_ := q_i == q_j. Joins the solutions for q_i and q_j.
// 
// Return true if propEq modified the binding of (q_i:C) or the binding
// of (q_j:K) if _Dyn_ was of the form q_i == q_k.
bool
Constraints::propEq(EnvironmentMap &E, Eq *Dyn, EnvironmentMap::iterator &CurValLHS) {
  bool ChangedEnv = false;

 if (VarAtom *RHSVar = dyn_cast<VarAtom>(Dyn->getRHS())) {
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

// Given an equality constraint _Dyn_, and a current variable binding
// _CurValLHS_, where _CurValLHS_ represents the pair (q_i:C) and the
// equality constraint _Dyn_ := q_i == K; if K <: T then update the solution.
//
// T is constrained to be one of the types from the constant lattice.
// T is parametric because the logic for equality propagation is common
// between different cases of constraint solving.
//
// Return true if propEq modified the binding of (q_i:C).
template <typename T>
bool Constraints::propGeq(Geq *Dyn, T *A, ConstraintSet &R,
                          EnvironmentMap::iterator &CurValLHS) {
    bool ChangedEnv = false;

    if (isa<T>(Dyn->getRHS())) {
        if (*(CurValLHS->second) < *A) {
            R.insert(Dyn);
            ChangedEnv = assignConstToVar(CurValLHS, A);
        }
    }

    return ChangedEnv;
}

// Propagates implication through the environment for a single 
// variable (whose value is given by _V_) used in an implication 
// constraint _Imp_.
template <typename T>
bool
Constraints::propImp(Implies *Imp, T *A, ConstraintSet &R, ConstAtom *V) {
  Geq *P = Imp->getPremise();
  if (isa<T>(P->getRHS()) && *V == *A) {
    Constraint *Con = Imp->getConclusion();
    R.insert(Imp);
    addConstraint(Con);
    return true;
  }
  return false;
}

// Takes one iteration to solve the system of constraints. Each step 
// involves the propagation of quantifiers and the potential firing of
// implications. Updates the global environment map.
//
// Returns true if the step didn't change any bindings of variables in
// the environment. 
bool Constraints::step_solve_old(void) {
  bool ChangedEnv = false;

  EnvironmentMap &env = environment.getVariables();
  EnvironmentMap::iterator VI = env.begin();
  // Step 1. Propagate any WILD constraint as far as we can.
  while (VI != env.end()) {
    // Iterate over the environment, VI is a pair of a variable q_i and 
    // the constant (one of Ptr, Arr, Wild) that the variable is bound to.
    VarAtom *Var = VI->first;
    ConstAtom *Val = VI->second;
    
    ConstraintSet rmConstraints;
    for (const auto &C : Var->Constraints) 
      if (Eq *E = dyn_cast<Eq>(C))
        ChangedEnv |= propEq(env, E, VI);
      else if (Geq *E = dyn_cast<Geq>(C))
          ChangedEnv |= propGeq<WildAtom>(E, getWild(), rmConstraints, VI);
      else if (Implies *Imp = dyn_cast<Implies>(C))
        ChangedEnv |= propImp<WildAtom>(Imp, getWild(),rmConstraints, Val);

    for (const auto &RC : rmConstraints)
      Var->eraseConstraint(RC);

    ++VI;
  }

  VI = env.begin();
  // Step 2. Propagate any ARR constraints.
  while (VI != env.end()) {
    VarAtom *Var = VI->first;

    ConstraintSet RemCons;
    for (const auto &C : Var->Constraints) {
      // Re-read the assignment as the propagating might have
      // changed this and the constraints will get removed.
      ConstAtom *Val = VI->second;
      if (Geq *E = dyn_cast<Geq>(C)) {
          ChangedEnv |= propGeq<NTArrAtom>(E, getNTArr(), RemCons, VI);
          ChangedEnv |= propGeq<ArrAtom>(E, getArr(), RemCons, VI);
      } else if (Eq *E = dyn_cast<Eq>(C)) {
          ChangedEnv |= propEq(env, E, VI);
      } else if (Implies *Imp = dyn_cast<Implies>(C)) {
        ChangedEnv |= propImp<NTArrAtom>(Imp, getNTArr(),
                                                 RemCons, Val);
        ChangedEnv |= propImp<ArrAtom>(Imp, getArr(),
                                               RemCons, Val);
      }
    }

    for (const auto &RC : RemCons)
      Var->eraseConstraint(RC);

    ++VI;
  }

  return !ChangedEnv;
}

// Solving algorithm. Produces the least solution according to ptr < arr < ntarr < wild.
//
//Constraints have form
//
//k = k’
//k >= q
//k > q ==> k’ > q’
//
//Til fixpoint
//  For all k >= q constraints, set sol(k) = q. Remove these constraints
//  For all k = k’ constraints, propagate solutions.

Constraint *Constraints::solve_new(unsigned &Niter) {
    bool ChangedEnv = true;
    bool NotFixedPoint = true;
    Niter = 0;
    EnvironmentMap::iterator VI;
    EnvironmentMap &env = environment.getVariables();

    // Proper solving
    while (ChangedEnv) {
        ChangedEnv = false;
        Niter++;

        // Step 1. Propagate any Geq(v,c) constraints, which can be summarily
        // deleted
        VI = env.begin();
        while (VI != env.end()) {
            VarAtom *Var = VI->first;
            ConstraintSet RemCons;
            for (const auto &C : Var->Constraints) {
                if (Geq *GE = dyn_cast<Geq>(C)) {
                    VarAtom *VA = dyn_cast<VarAtom>(GE->getLHS());
                    ConstAtom *CA = dyn_cast<ConstAtom>(GE->getRHS());
                    if (CA == nullptr) continue;

                    EnvironmentMap::iterator CurVal = env.find(VA);
                    assert(CurVal != env.end()); // The var on the RHS should be in the env.

                    if (*(CurVal->second) < *CA) {
                        ChangedEnv |= assignConstToVar(CurVal, CA);
                    }
                    RemCons.insert(GE);
                }
            }
            for (const auto &RC : RemCons)
                Var->eraseConstraint(RC);
            VI++;
        }

        // Step 2. Propagate any Eq(v,v) or Geq(v,v) or Geq(c,v) constraints
        //   Go until a fixed point reached -- warning, is quadratic (want graph)
        NotFixedPoint = true;
        while (NotFixedPoint) {
            NotFixedPoint = false;
            VI = env.begin();
            while (VI != env.end()) {
                VarAtom *Var = VI->first;
                for (const auto &C : Var->Constraints) {
                    VarAtom *lhs, *rhs;
                    ConstAtom *c = nullptr;
                    EnvironmentMap::iterator CurValLHS, CurValRHS;
                    int isEq = 0;
                    if (Eq *E = dyn_cast<Eq>(C)) {
                        isEq = 1; // EQ
                        lhs = dyn_cast<VarAtom>(E->getLHS());
                        rhs = dyn_cast<VarAtom>(E->getRHS());
                        assert(lhs != nullptr);
                    } else if (Geq *E = dyn_cast<Geq>(C)) {
                        isEq = 2; // GEQ
                        lhs = dyn_cast<VarAtom>(E->getLHS());
                        rhs = dyn_cast<VarAtom>(E->getRHS());
                        if (lhs == nullptr) c = dyn_cast<ConstAtom>(E->getLHS());
                    } else {
                        continue;
                    }
                    assert(rhs != nullptr); // Should not have ConstAtoms on rhs, after step 1
                    CurValRHS = env.find(rhs);
                    assert(CurValRHS != env.end());

                    if (c != nullptr) { // Geq(c,v) -- check that the inequality holds
                        if (*c < *(CurValRHS->second)) {
                            return C; // failed on this constraint, so we return that
                        }
                        // else it's OK
                    }
                    else {
                        CurValLHS = env.find(lhs);
                        assert(CurValLHS != env.end());
                        if (isEq) { // have Geq(v,v) or Eq(v,v)
                            if (*(CurValLHS->second) < *(CurValRHS->second)) {
                                NotFixedPoint |= assignConstToVar(CurValLHS, CurValRHS->second);
                            }
                        }
                        if (isEq == 1) { // have Eq(v,v), so check the other direction too
                            if (*(CurValRHS->second) < *(CurValLHS->second)) {
                                NotFixedPoint |= assignConstToVar(CurValRHS, CurValLHS->second);;
                            }
                        }
                    }
                }
                VI++;
            }
            ChangedEnv |= NotFixedPoint;
        }

        // Step 3. Propagate implications
        VI = env.begin();
        while (VI != env.end()) {
            VarAtom *Var = VI->first;
            ConstraintSet RemCons;
            for (const auto &C : Var->Constraints) {
                if (Implies *Imp = dyn_cast<Implies>(C)) {
                    Geq *premise = Imp->getPremise();
                    VarAtom *lhs = dyn_cast<VarAtom>(premise->getLHS());
                    ConstAtom *rhs = dyn_cast<ConstAtom>(premise->getRHS());
                    assert(lhs != nullptr && rhs != nullptr);
                    EnvironmentMap::iterator CurValLHS = env.find(lhs);
                    assert(CurValLHS != env.end());

                    if (*(CurValLHS->second) == *rhs || *rhs < *(CurValLHS->second)) {
                        Constraint *Con = Imp->getConclusion();
                        addConstraint(Con);
                        RemCons.insert(Imp); // delete it; won't need anymore
                        ChangedEnv = true;
                    }
                }
            }
            for (const auto &RC : RemCons)
                Var->eraseConstraint(RC);
            VI++;
        }

        if (DebugSolver) {
            errs() << "constraints after iter #" << Niter << "\n";
            dump();
        }

    }

    return nullptr;
}

// Make a graph G:
//- with nodes for each variable k and each qualifier constant q.
//- with edges Q --> Q’ for each constraint Q <: Q’
// Note: Constraints (q <: k ⇒ q’ <: k’) are not supported, but we shouldn’t
// actually need them. So make your algorithm die if it comes across them.
//
// For each non-constant node k in G,
//- set sol(k) = q_\bot (the least element, i.e., Ptr)
//
// For each constant node q_i, starting with the highest and working down,
//- set worklist W = { q_i }
//- while W nonempty
//-- let Q = take(W)
//-- For all edges (Q --> k) in G
//--- if sol(k) <> (sol(k) JOIN Q) then
//---- set sol(k) := (sol(k) JOIN Q)
//---- for all edges (k --> q) in G, confirm that sol(k) <: q; else fail
//---- add k to W

bool Constraints::graph_based_solve(unsigned &Niter) {
  ConstraintsGraph ChkCG;
  //ConstraintsGraph PtrTypCG;
  std::set<Implies *> SavedImplies;
  ConstraintsEnv &env = environment;

  // Setup the Constraint Graph.
  for (const auto &C : constraints) {
    if (Eq *E = dyn_cast<Eq>(C)) {
      ChkCG.addConstraint(E, *this);
      //PtrTypCG.addConstraint(E, *this);
    }
    else if (Geq *G = dyn_cast<Geq>(C)) {
      // if (G->constraintIsChecked())
      ChkCG.addConstraint(G, *this);
      // else
      //   PtrTypCG.addConstraint(G, *this);
    }
    // Save the implies to solve them later.
    else if (Implies *Imp = dyn_cast<Implies>(C)) {
      SavedImplies.insert(Imp);
    }
    else
      llvm_unreachable("Bogus constraint type");
  }

  // Solving
  if (DebugSolver)
    ChkCG.dumpCGDot("constraints_graph.dot");

  // Initialize work list with ConstAtoms.
  std::vector<Atom *> WorkList;
  std::set<Implies *> FiredImplies;
  do {
    WorkList.clear();
    auto &InitC = ChkCG.getAllConstAtoms();
    WorkList.insert(WorkList.begin(), InitC.begin(), InitC.end());

    while (!WorkList.empty()) {
      auto *CurrAtom = *(WorkList.begin());
      // Remove the first element.
      WorkList.erase(WorkList.begin());

      // Get the solution of the CurrAtom.
      ConstAtom *CurrSol = env.getAssignment(CurrAtom);

      std::set<Atom *> Successors;
      // get successors
      ChkCG.getSuccessors<VarAtom>(CurrAtom, Successors);
      for (auto *SucA : Successors) {
        bool Changed = false;
        /*llvm::errs() << "Sucessor:" << SucA->getStr()
                     << " of " << CurrAtom->getStr() << "\n";*/
        if (VarAtom *K = dyn_cast<VarAtom>(SucA)) {
          ConstAtom *SucSol = env.getAssignment(K);
          // --- if sol(k) <> (sol(k) JOIN Q) then
          if (*SucSol < *CurrSol) {
            // ---- set sol(k) := (sol(k) JOIN Q)
            Changed = env.assign(K,CurrSol);
            /*if (Changed) {
              llvm::s()err << "Trying to assign:" << CurrSol->getStr() << " to "
                           << K->getStr() << "\n";
            }*/
          }
          if (Changed) {
            // get the latest assignment.
            SucSol = env.getAssignment(K);
            // ---- for all edges (k --> q) in G, confirm
            std::set<Atom *> KSuccessors;
            ChkCG.getSuccessors<ConstAtom>(K, KSuccessors);
            for (auto *KChild : KSuccessors) {
              ConstAtom *KCSol = env.getAssignment(KChild);
              // that sol(k) <: q; else fail
              if (!(*SucSol < *KCSol) && *SucSol != *KCSol) {
                // failure case.
                errs() << "Invalid graph formed on Vertex:";
                SucSol->print(errs());
                KCSol->print(errs());
                K->print(errs());
                return false;
              }
            }
            // ---- add k to W
            WorkList.push_back(K);
          }
        }
      }
    }
    Niter++;
    FiredImplies.clear();

    // If there are some implications that we saved? Propagate them.
    if (!SavedImplies.empty()) {
      // Check if Premise holds? If yes then fire the conclusion.
      for (auto *Imp : SavedImplies) {
        Geq *Pre = Imp->getPremise();
        Geq *Con = Imp->getConclusion();
        ConstAtom *Cca = env.getAssignment(Pre->getRHS());
        ConstAtom *Cva = env.getAssignment(Pre->getLHS());
        // Premise is true, so fire the conclusion.
        if (*Cca < *Cva || *Cca == *Cva) {
          /*llvm::errs() << "Firing Conclusion:";
          Con->print(llvm::errs());
          llvm::errs() << "\n";*/
          // FIXME: Can be smarter by adding only the Con's LHS VarAtom to the worklist
          ChkCG.addConstraint(Con, *this);
          // Keep track of fired constraints, so that we can delete them.
          FiredImplies.insert(Imp);
        }
      }
      // Erase all the fired implies.
      for (auto *ToDel : FiredImplies) {
        SavedImplies.erase(ToDel);
      }
    }
    // Lets repeat if there are some fired constraints.
  } while (!FiredImplies.empty());

  return true;
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

  if (UseOldSolver) {
      while (Fixed == false) {

          if (DebugSolver) {
              errs() << "constraints pre step\n";
              dump();
          }

          Fixed = step_solve_old();

          if (DebugSolver) {
              errs() << "constraints post step\n";
              dump();
          }

          NumOfIter++;
      }
  }
  else { /* New Solver */
      //Constraint *C = solve_new(NumOfIter);
      //assert(C == nullptr); // eventually this could happen, so we have to report the error
      graph_based_solve(NumOfIter);
  }

  return std::pair<Constraints::ConstraintSet, bool>(Conflicts, true);
}

void Constraints::print(raw_ostream &O) const {
  O << "CONSTRAINTS: \n";
  for (const auto &C : constraints) {
    C->print(O);
    O << "\n";
  }
  environment.print(O);
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

  environment.dump_json(O);
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
  return environment.getOrCreateVar(V,getPtr());
}

VarAtom *Constraints::getVar(uint32_t V) const {
  return environment.getVar(V);
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
  return environment.getAssignment(V);
}

ConstAtom *Constraints::getAssignment(Atom *A) {
  return environment.getAssignment(A);
}

bool Constraints::isWild(uint32_t V) {
  return dyn_cast<WildAtom>(environment.getAssignment(V)) != nullptr;
}

bool Constraints::isWild(Atom *A) {
  return dyn_cast<WildAtom>(environment.getAssignment(A)) != nullptr;
}

Geq *Constraints::createGeq(Atom *Lhs, Atom *Rhs, bool isCheckedConstraint) {
    return new Geq(Lhs, Rhs, isCheckedConstraint);
}

Constraint *Constraints::createEq(Atom *Lhs, Atom *Rhs) {
  VarAtom *VAlhs = dyn_cast<VarAtom>(Lhs);
  VarAtom *VArhs = dyn_cast<VarAtom>(Rhs);
  if (VAlhs != nullptr && VArhs != nullptr)
    return new Eq(VAlhs, VArhs);
  else
    return Constraints::createGeq(Lhs, Rhs);
}

Geq *Constraints::createGeq(Atom *Lhs, Atom *Rhs, std::string &Rsn, bool isCheckedConstraint) {
    return new Geq(Lhs, Rhs, Rsn, isCheckedConstraint);
}

Constraint *Constraints::createEq(Atom *Lhs, Atom *Rhs, std::string &Rsn) {
  VarAtom *VAlhs = dyn_cast<VarAtom>(Lhs);
  VarAtom *VArhs = dyn_cast<VarAtom>(Rhs);
  if (VAlhs != nullptr && VArhs != nullptr)
    return new Eq(VAlhs, VArhs, Rsn);
  return Constraints::createGeq(Lhs,Rhs,Rsn);
}

Geq *Constraints::createGeq(Atom *Lhs, Atom *Rhs, std::string &Rsn,
                                  PersistentSourceLoc *PL, bool isCheckedConstraint) {
    if (PL != nullptr && PL->valid()) {
        // Make this invalid, if the source location is not absolute path
        // this is to avoid crashes in clangd.
        if (PL->getFileName().c_str()[0] != '/')
            PL = nullptr;
    }
    return new Geq(Lhs, Rhs, Rsn, PL, isCheckedConstraint);
}

Constraint *Constraints::createEq(Atom *Lhs, Atom *Rhs, std::string &Rsn,
                          PersistentSourceLoc *PL) {
  VarAtom *VAlhs = dyn_cast<VarAtom>(Lhs);
  VarAtom *VArhs = dyn_cast<VarAtom>(Rhs);
  if (VAlhs != nullptr && VArhs != nullptr) {
      if (PL != nullptr && PL->valid()) {
          // Make this invalid, if the source location is not absolute path
          // this is to avoid crashes in clangd.
          if (PL->getFileName().c_str()[0] != '/')
              PL = nullptr;
      }
      return new Eq(VAlhs, VArhs, Rsn, PL);
  }
  else
    return Constraints::createGeq(Lhs,Rhs,Rsn,PL);
}

Implies *Constraints::createImplies(Geq *Premise,
                                    Geq *Conclusion) {
  return new Implies(Premise, Conclusion);
}

void Constraints::resetConstraints() {
  // Update all constraints to pointers.
  environment.resetSolution(getPtr());
}

bool Constraints::checkInitialEnvSanity() {
  // All variables should be Ptrs.
  for (const auto &EnvVar : environment.getVariables()) {
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

/* ConstraintsEnv methods */

void ConstraintsEnv::dump(void) const {
  print(errs());
}

void ConstraintsEnv::print(raw_ostream &O) const {
  O << "ENVIRONMENT: \n";
  for (const auto &V : environment) {
    V.first->print(O);
    O << " = ";
    V.second->print(O);
    O << "\n";
  }
}

void ConstraintsEnv::dump_json(llvm::raw_ostream &O) const {
  bool addComma = false;
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

VarAtom *ConstraintsEnv::getOrCreateVar(uint32_t V, ConstAtom *initC) {
  VarAtom Tv(V);
  EnvironmentMap::iterator I = environment.find(&Tv);

  if (I != environment.end())
    return I->first;
  else {
    VarAtom *V = new VarAtom(Tv);
    environment[V] = initC;
    return V;
  }
}

VarAtom *ConstraintsEnv::getVar(uint32_t V) const {
  VarAtom Tv(V);
  EnvironmentMap::const_iterator I = environment.find(&Tv);

  if (I != environment.end())
    return I->first;
  else
    return nullptr;
}

ConstAtom *ConstraintsEnv::getAssignment(uint32_t V) {
  auto CurrVar = getVar(V);
  assert(CurrVar != nullptr && "Queried uncreated constraint variable.");
  return environment[CurrVar];
}

ConstAtom *ConstraintsEnv::getAssignment(Atom *A) {
  if (VarAtom *VA = dyn_cast<VarAtom>(A)) {
    return environment[VA];
  }
  assert(dyn_cast<ConstAtom>(A) != nullptr &&
         "This is not a VarAtom or ConstAtom");
  return dyn_cast<ConstAtom>(A);
}

bool ConstraintsEnv::assign(VarAtom *V, ConstAtom *C) {
  auto VI = environment.find(V);
  if (VI->first->canAssign(C)) {
    VI->second = C;
    return true;
  }
  return false;
}

void ConstraintsEnv::resetSolution(ConstAtom *initC) {
  for (auto &CurrE : environment) {
    CurrE.second = initC;
  }
}
