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
#include <clang/CConv/ConstraintVariables.h>

using namespace llvm;

static cl::OptionCategory SolverCategory("solver options");
static cl::opt<bool> DebugSolver("debug-solver",
  cl::desc("Dump intermediate solver state"),
  cl::init(false), cl::cat(SolverCategory));

Constraint::Constraint(ConstraintKind K, const std::string &Rsn,
                       PersistentSourceLoc *PL): Constraint(K, Rsn) {
  if (PL != nullptr && PL->valid()) {
    FileName = PL->getFileName();
    LineNo = PL->getLineNo();
    ColStart = PL->getColSNo();
    ColEnd = PL->getColENo();
  }
}

// Remove the constraint from the global constraint set.
bool Constraints::removeConstraint(Constraint *C) {
  bool RetVal = false;
  Geq *GE = dyn_cast<Geq>(C);
  assert(GE != nullptr && "Invalid constrains requested to be removed.");
  // We can only remove constraints from ConstAtoms.
  if (isa<ConstAtom>(GE->getRHS()) &&
      isa<VarAtom>(GE->getLHS())) {
    removeReasonBasedConstraint(C);
    RetVal = constraints.erase(C) != 0;
    // Delete from graph.
    ConstraintsGraph *TG = nullptr;
    TG = GE->constraintIsChecked() ? ChkCG : PtrTypCG;
    RetVal = true;
    // Remove the edge form the corresponding constraint graph.
    TG->removeEdge(GE->getRHS(), GE->getLHS());
  }
  return RetVal;
}

// Check if we can add this constraint. This provides a global switch to
// control what constraints we can add to our system.
void Constraints::editConstraintHook(Constraint *C) {
  if (!AllTypes) {
    // Invalidate any pointer-type constraints.
    if (Geq *E = dyn_cast<Geq>(C)) {
      if (!E->constraintIsChecked()) {
        VarAtom *LHSA = dyn_cast<VarAtom>(E->getLHS());
        VarAtom *RHSA = dyn_cast<VarAtom>(E->getRHS());
        if (LHSA != nullptr && RHSA != nullptr) {
          return;
        }
        // Make this checked only if the const atom is other than Ptr.
        if (RHSA) {
          if (!dyn_cast<PtrAtom>(E->getLHS())) {
            E->setChecked(getWild());
            E->setReason(POINTER_IS_ARRAY_REASON);
          }
        } else {
          assert (LHSA && "Adding constraint between constants?!");
          if (!dyn_cast<PtrAtom>(E->getRHS())) {
            E->setChecked(getWild());
            E->setReason(POINTER_IS_ARRAY_REASON);
          }
        }
      }
    }
  }
}

// Add a constraint to the set of constraints. If the constraint is already 
// present (by syntactic equality) return false. 
bool Constraints::addConstraint(Constraint *C) {
  // Validate the constraint to be added.
  if (!check(C)) {
    C->dump();
    assert(false);
  }

  editConstraintHook(C);

  // Check if C is already in the set of constraints. 
  if (constraints.find(C) == constraints.end()) {
    constraints.insert(C);

    if (Geq *G = dyn_cast<Geq>(C)) {
      if (G->constraintIsChecked())
        ChkCG->addConstraint(G, *this);
      else
        PtrTypCG->addConstraint(G, *this);
    }

    addReasonBasedConstraint(C);

    // Update the variables that depend on this constraint.
    if (Geq *E = dyn_cast<Geq>(C)) {
      if (VarAtom *vLHS = dyn_cast<VarAtom>(E->getLHS()))
        vLHS->Constraints.insert(C);
      else if (VarAtom *vRHS = dyn_cast<VarAtom>(E->getRHS())) {
        vRHS->Constraints.insert(C);
      }
    }
    else if (Implies *I = dyn_cast<Implies>(C)) {
      Geq *PEQ = I->getPremise();
      if (VarAtom *vLHS = dyn_cast<VarAtom>(PEQ->getLHS()))
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
  if (Geq *E = dyn_cast<Geq>(C)) {
      if (E->getReason() != DEFAULT_REASON && !E->getReason().empty())
          return this->constraintsByReason[E->getReason()].insert(E).second;
  }
  return false;
}

bool Constraints::removeReasonBasedConstraint(Constraint *C) {
  if (Geq *E = dyn_cast<Geq>(C)) {
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
// IMPLIES : (q_i >= A) => (q_k >= B) for A,B constant.
bool Constraints::check(Constraint *C) {

  if (Implies *I = dyn_cast<Implies>(C)) {
    Geq *P = I->getPremise();
    Geq *CO = I->getConclusion();
    if (!isa<VarAtom>(P->getLHS()) || isa<VarAtom>(P->getRHS()) ||
        !isa<VarAtom>(CO->getLHS()) || isa<VarAtom>(CO->getRHS()))
      return false;
  }
  else if (dyn_cast<Geq>(C) != nullptr) {
      // all good!
  }
  else
    return false; // Not Eq, Geq, or Implies; what is it?!

  return true;
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

static bool do_solve(ConstraintsGraph &CG,
                     std::set<Implies *> SavedImplies, // TODO: Can this be a ref?
                     ConstraintsEnv & env,
                     Constraints *CS, bool doLeastSolution,
                     std::set<VarAtom *> *InitVs,
                     std::set<VarAtom *> &Conflicts) {

  std::vector<Atom *> WorkList;
  std::set<Implies *> FiredImplies;

  // Initialize with seeded VarAtom set (pre-solved).
  if (InitVs != nullptr)
    WorkList.insert(WorkList.begin(), InitVs->begin(), InitVs->end());

  do {
    // Initialize work list with ConstAtoms.
    auto &InitC = CG.getAllConstAtoms();
    WorkList.insert(WorkList.begin(), InitC.begin(), InitC.end());

    while (!WorkList.empty()) {
      auto *Curr = *(WorkList.begin());
      // Remove the first element, get its solution.
      WorkList.erase(WorkList.begin());
      ConstAtom *CurrSol = env.getAssignment(Curr);

      // get its neighbors.
      std::set<Atom *> Neighbors;
      CG.getNeighbors(Curr, Neighbors, doLeastSolution);
      // update each successor's solution.
      for (auto *NeighborA : Neighbors) {
        bool Changed = false;
        if (VarAtom *Neighbor = dyn_cast<VarAtom>(NeighborA)) {
          ConstAtom *NghSol = env.getAssignment(Neighbor);
          // update solution if doing so would change it
          // checked? --- if sol(Neighbor) <> (sol(Neighbor) JOIN Cur)
          //   else   --- if sol(Neighbor) <> (sol(Neighbor) MEET Cur)
          if ((doLeastSolution && *NghSol < *CurrSol) ||
              (!doLeastSolution && *CurrSol < *NghSol)) {
            // ---- set sol(k) := (sol(k) JOIN/MEET Q)
            Changed = env.assign(Neighbor, CurrSol);
            assert (Changed);
            WorkList.push_back(Neighbor);
          }
        } // ignore ConstAtoms for now; will confirm solution below
      }
    }
    FiredImplies.clear();

    // If there are some implications that we saved? Propagate them.
    if (!SavedImplies.empty()) {
      // Check if Premise holds. If yes then fire the conclusion.
      for (auto *Imp : SavedImplies) {
        Geq *Pre = Imp->getPremise();
        Geq *Con = Imp->getConclusion();
        ConstAtom *Cca = env.getAssignment(Pre->getRHS());
        ConstAtom *Cva = env.getAssignment(Pre->getLHS());
        // Premise is true, so fire the conclusion.
        if (*Cca < *Cva || *Cca == *Cva) {
          CG.addConstraint(Con, *CS);
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

  // Check Upper/lower bounds hold; collect failures in conflicts set.
  std::set<Atom *> Neighbors;
  bool ok = true;
  for (ConstAtom *Cbound : CG.getAllConstAtoms()) {
    if (CG.getNeighbors(Cbound, Neighbors, !doLeastSolution)) {
      for (Atom *A : Neighbors) {
        VarAtom *VA = dyn_cast<VarAtom>(A);
        if (VA == nullptr)
          continue;
        ConstAtom *Csol = env.getAssignment(VA);
        if ((doLeastSolution && *Cbound < *Csol) ||
            (!doLeastSolution && *Csol < *Cbound)) {
          ok = false;
          // Save effected VarAtom in conflict set. This will be constrained to
          // wild after pointer type solving is finished.
          Conflicts.insert(VA);
          // Failure case.
          if (Verbose) {
            errs() << "Unsolvable constraints: ";
            VA->print(errs());
            errs() << "=";
            Csol->print(errs());
            errs() << (doLeastSolution? "<" : ">");
            Cbound->print(errs());
            errs() << " var will be made WILD\n";
          }
        }
      }
    }
  }

  return ok;
}

VarAtomPred isReturn = [](VarAtom *VA) -> bool {
  return VA->getVarKind() == VarAtom::V_Return;
};
VarAtomPred isParam = [](VarAtom *VA) -> bool {
  return VA->getVarKind() == VarAtom::V_Param;
};
VarAtomPred isNonParamReturn = [](VarAtom *VA) -> bool {
  return !isReturn(VA) && !isParam(VA);
};

// Remove from S all elements that don't match the predicate P
void filter(VarAtomPred P, std::set<VarAtom *> &S) {
  for (auto I = S.begin(), E = S.end(); I != E;) {
    if (!P(*I))
      I = S.erase(I);
    else
      ++I;
  }
}

// For the provided constraint graph, construct the set of atoms bounded in a
// direction (defined by Succs) by an atom from the set of concrete atoms.
// When Succs is true, the traversal flows from each node to its successor - in
// the direction the edges are oriented. When false, the traversal is reversed.
// To view this another way, true checks for a lower bound in the Ptyp
// constraint graph, but an upper bound in the checked graph. UseConstAtoms
// decides if constant atoms should be used in addition to the provided Concrete
// atoms.
static std::set<VarAtom *> findBounded(ConstraintsGraph &CG,
                                       std::set<VarAtom *> *Concrete,
                                       bool Succs, bool UseConstAtoms = true) {
  std::set<VarAtom *> Bounded;
  std::set<Atom *> Open;

  // Initialize the open set of atoms with the provided set of fixed atoms.
  // These are the start points for a traversal of the constraint graph.
  if (Concrete != nullptr) {
    Open.insert(Concrete->begin(), Concrete->end());
    Bounded.insert(Concrete->begin(), Concrete->end());
  }

  // We often, but not always, want to consider constant atoms as concrete.
  if (UseConstAtoms) {
    auto &ConstA = CG.getAllConstAtoms();
    Open.insert(ConstA.begin(), ConstA.end());
  }

  // Traversal of the constraint graph. An atom is bounded in a direction by
  // one of the Concrete atoms if it is reachable from one of the atoms taking
  // only edges in that direction. The particular atom bounding it does not
  // matter.
  while (!Open.empty()) {
    auto *Curr = *(Open.begin());
    Open.erase(Open.begin());

    std::set<Atom *> Neighbors;
    CG.getNeighbors(Curr, Neighbors, Succs);
    for (Atom *A : Neighbors) {
      VarAtom *VA = dyn_cast<VarAtom>(A);
      if (VA && Bounded.find(VA) == Bounded.end()) {
        Open.insert(VA);
        Bounded.insert(VA);
      }
    }
  }

  return Bounded;
}

bool Constraints::graph_based_solve(std::set<VarAtom *> &Conflicts) {
  ConstraintsGraph SolChkCG;
  ConstraintsGraph SolPtrTypCG;
  std::set<Implies *> SavedImplies;
  std::set<Implies *> Empty;
  ConstraintsEnv &env = environment;

  // Checked well-formedness.
  environment.checkAssignment(getDefaultSolution());

  // Setup the Checked Constraint Graph.
  for (const auto &C : constraints) {
    if (Geq *G = dyn_cast<Geq>(C)) {
      if (G->constraintIsChecked())
        SolChkCG.addConstraint(G, *this);
      else
        SolPtrTypCG.addConstraint(G, *this);
    }
    // Save the implies to solve them later.
    else if (Implies *Imp = dyn_cast<Implies>(C)) {
      assert(Imp->getConclusion()->constraintIsChecked() &&
          Imp->getPremise()->constraintIsChecked());
      SavedImplies.insert(Imp);
    }
  }

  if (DebugSolver)
    GraphVizOutputGraph::dumpConstraintGraphs("initial_constraints_graph.dot",
                                              SolChkCG, SolPtrTypCG);

  // Solve Checked/unchecked constraints first.
  env.doCheckedSolve(true);
  
  bool res = do_solve(SolChkCG, SavedImplies, env, this, true, nullptr, Conflicts);

  // Now solve PtrType constraints
  if (res && AllTypes) {
    env.doCheckedSolve(false);

    // Step 1: Greatest solution
    res = do_solve(SolPtrTypCG, Empty, env, this, false, nullptr, Conflicts);


    // Step 2: Reset all solutions but for function params,
    // and compute the least.
    if (res) {

      // We want to find all local variables with an upper bound that provide a
      // lower bound for return variables that are not otherwise bounded.

      // 1. Find return vars with a lower bound.
      std::set<VarAtom *> ParamVars = env.filterAtoms(isParam);
      std::set<VarAtom *> LowerBoundedRet =
          findBounded(SolPtrTypCG, &ParamVars, true);
      filter(isReturn, LowerBoundedRet);

      // 2. Find local vars where one of the return vars is an upper bound.
      //    Conversely, these are an alternative lower bound for the return var.
      std::set<VarAtom *> RetUpperBoundedLocals =
          findBounded(SolPtrTypCG, &LowerBoundedRet, false, false);
      filter(isNonParamReturn, RetUpperBoundedLocals);

      // 3. Find local vars upper bounded by a const var.
      std::set<VarAtom *> ConstUpperBoundedLocals =
          findBounded(SolPtrTypCG, nullptr, false);
      filter(isNonParamReturn, ConstUpperBoundedLocals);

      // 4. Take set difference of 2 and 3 to find bounded vars that do not
      //    effect an existing lower bound.
      std::set<VarAtom *> Diff;
      std::set_difference(
          ConstUpperBoundedLocals.begin(), ConstUpperBoundedLocals.end(),
          RetUpperBoundedLocals.begin(), RetUpperBoundedLocals.end(),
          std::inserter(Diff, Diff.begin()));

      // 5. Reset var to NTArr if not a param var and not in the previous set.
      std::set<VarAtom *> rest = env.resetSolution(
          [Diff](VarAtom *VA) -> bool {
            return !(isParam(VA) || Diff.find(VA) != Diff.end());
          },
          getNTArr());

      // Remember which variables have a concrete lower bound. Variables without
      // a lower bound will be resolved in the final greatest solution.
      std::set<VarAtom *> LowerBounded =
          findBounded(SolPtrTypCG, &rest, true);

      res = do_solve(SolPtrTypCG, Empty, env, this, true, &rest, Conflicts);

      // Step 3: Reset local variable solutions, compute greatest
      if (res) {
        rest.clear();

        rest = env.resetSolution(
            [LowerBounded](VarAtom *VA) -> bool {
              return isNonParamReturn(VA) ||
                  LowerBounded.find(VA) == LowerBounded.end();
            },
            getPtr());

        res = do_solve(SolPtrTypCG, Empty, env, this, false, &rest,
                       Conflicts);
      }
    }
    // If PtrType solving (partly) failed, make the affected VarAtoms wild.
    if (!res) {
      std::set<VarAtom *> rest;
      env.doCheckedSolve(true);
      for (VarAtom *VA : Conflicts) {
        assert(VA != nullptr);
        std::string Rsn = "Bad pointer type solution";
        Geq *ConflictConstraint = createGeq(VA, getWild(), Rsn);
        addConstraint(ConflictConstraint);
        SolChkCG.addConstraint(ConflictConstraint, *this);
        rest.insert(VA);
      }
      Conflicts.clear();
      /* FIXME: Should we propagate the old res? */
      res = do_solve(SolChkCG, SavedImplies, env, this, true, &rest,
                     Conflicts);

    }
    // Final Step: Merge ptyp solution with checked solution.
    env.mergePtrTypes();
  }

  if (DebugSolver)
    GraphVizOutputGraph::dumpConstraintGraphs(
        "implication_constraints_graph.dot", SolChkCG, SolPtrTypCG);

  return res;
}

// Solve the system of constraints. Return true in the second position if
// the system is solved. If the system is solved, the first position is
// an empty. If the system could not be solved, the constraints in conflict
// are returned in the first position.
// TODO: Returns copy of conflicts set. Can we either get rid of this return
//       (It's not used as far as I can tell), or dynamically allocate and
//       return a pointer.
std::pair<std::set<VarAtom *>, bool> Constraints::solve() {

  std::set<VarAtom *> Conflicts;

  if (DebugSolver) {
    errs() << "constraints beginning solve\n";
    dump();
  }
  bool ok = graph_based_solve(Conflicts);

  if (DebugSolver) {
    errs() << "solution, when done solving\n";
    environment.dump();
  }

  return std::make_pair(Conflicts, ok);
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

VarAtom *Constraints::getOrCreateVar(ConstraintKey V, std::string Name,
                                     VarAtom::VarKind VK) {
  return environment.getOrCreateVar(V, getDefaultSolution(), Name, VK);
}

VarSolTy Constraints::getDefaultSolution() {
  return std::make_pair(getPtr(), getPtr());
}

VarAtom *Constraints::getFreshVar(std::string Name, VarAtom::VarKind VK) {
  return environment.getFreshVar(getDefaultSolution(), Name, VK);
}

VarAtom *Constraints::getVar(ConstraintKey V) const {
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

ConstAtom *Constraints::getAssignment(Atom *A) {
  environment.doCheckedSolve(true);
  return environment.getAssignment(A);
}

ConstraintsGraph &Constraints::getChkCG() {
  assert (ChkCG != nullptr &&
         "Checked Constraint graph cannot be nullptr");
  return *ChkCG;
}

ConstraintsGraph &Constraints::getPtrTypCG() {
  assert (PtrTypCG != nullptr && "Pointer type Constraint graph "
                                       "cannot be nullptr");
  return *PtrTypCG;
}

Geq *Constraints::createGeq(Atom *Lhs, Atom *Rhs, bool isCheckedConstraint) {
    return new Geq(Lhs, Rhs, isCheckedConstraint);
}

Geq *Constraints::createGeq(Atom *Lhs, Atom *Rhs, const std::string &Rsn,
                            bool isCheckedConstraint) {
    return new Geq(Lhs, Rhs, Rsn, isCheckedConstraint);
}

Geq *Constraints::createGeq(Atom *Lhs, Atom *Rhs, const std::string &Rsn,
                            PersistentSourceLoc *PL, bool isCheckedConstraint) {
    if (PL != nullptr && PL->valid()) {
        // Make this invalid, if the source location is not absolute path
        // this is to avoid crashes in clangd.
        if (PL->getFileName().c_str()[0] != '/')
            PL = nullptr;
    }
    return new Geq(Lhs, Rhs, Rsn, PL, isCheckedConstraint);
}

Implies *Constraints::createImplies(Geq *Premise,
                                    Geq *Conclusion) {
  return new Implies(Premise, Conclusion);
}

void Constraints::resetEnvironment() {
  environment.resetFullSolution(getDefaultSolution());
}

bool Constraints::checkInitialEnvSanity() {
  return environment.checkAssignment(getDefaultSolution());
}

Constraints::Constraints() {
  PrebuiltPtr = new PtrAtom();
  PrebuiltArr = new ArrAtom();
  PrebuiltNTArr = new NTArrAtom();
  PrebuiltWild = new WildAtom();
  ChkCG = new ConstraintsGraph();
  PtrTypCG = new ConstraintsGraph();
}

Constraints::~Constraints() {
  delete PrebuiltPtr;
  delete PrebuiltArr;
  delete PrebuiltNTArr;
  delete PrebuiltWild;
  if (ChkCG != nullptr)
    delete (ChkCG);
  if (PtrTypCG != nullptr)
    delete (PtrTypCG);
}

/* ConstraintsEnv methods */

void ConstraintsEnv::dump(void) const {
  print(errs());
}

void ConstraintsEnv::print(raw_ostream &O) const {
  O << "ENVIRONMENT: \n";
  for (const auto &V : environment) {
    V.first->print(O);
    O << " = [";
    O << "Checked=";
    V.second.first->print(O);
    O << ", PtrType=";
    V.second.second->print(O);
    O << "]";
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
    O << ", \"value:\":{\"checked\":";
    V.second.first->dump_json(O);
    O << ", \"PtrType\":";
    V.second.second->dump_json(O);
    O << "}}";
    addComma = true;
  }
  O << "]}";
}

VarAtom *ConstraintsEnv::getFreshVar(VarSolTy InitC, std::string Name,
                                     VarAtom::VarKind VK) {
  VarAtom *NewVA = getOrCreateVar(consFreeKey, InitC, Name, VK);
  consFreeKey++;
  return NewVA;
}

VarAtom *ConstraintsEnv::getOrCreateVar(ConstraintKey V, VarSolTy InitC,
                                        std::string Name, VarAtom::VarKind VK) {
  VarAtom Tv(V,Name,VK);
  EnvironmentMap::iterator I = environment.find(&Tv);

  if (I != environment.end())
    return I->first;
  else {
    VarAtom *VA = new VarAtom(Tv);
    environment[VA] = InitC;
    return VA;
  }
}

VarAtom *ConstraintsEnv::getVar(ConstraintKey V) const {
  VarAtom Tv(V);
  EnvironmentMap::const_iterator I = environment.find(&Tv);

  if (I != environment.end())
    return I->first;
  else
    return nullptr;
}


ConstAtom *ConstraintsEnv::getAssignment(Atom *A) {
  if (VarAtom *VA = dyn_cast<VarAtom>(A)) {
    if (useChecked) {
      return environment[VA].first;
    } else {
      return environment[VA].second;
    }
  }
  assert(dyn_cast<ConstAtom>(A) != nullptr &&
         "This is not a VarAtom or ConstAtom");
  return dyn_cast<ConstAtom>(A);
}

bool ConstraintsEnv::checkAssignment(VarSolTy sol) {
  for (const auto &EnvVar : environment) {
    if (EnvVar.second.first != sol.first || EnvVar.second.second != sol.second) {
      return false;
    }
  }
  return true;
}

bool ConstraintsEnv::assign(VarAtom *V, ConstAtom *C) {
  auto VI = environment.find(V);
  if (useChecked) {
    VI->second.first = C;
  } else {
    VI->second.second = C;
  }
  return true;
}

// Find VarAtoms in the environment that match a predicate.
std::set<VarAtom *> ConstraintsEnv::filterAtoms(VarAtomPred Pred) {
  std::set<VarAtom *> Matches;
  for (const auto &CurrE : environment) {
    VarAtom *VA = CurrE.first;
    if (Pred(VA))
      Matches.insert(VA);
  }
  return Matches;
}

// Reset solution of all VarAtoms that satisfy the given predicate
//  to be the given ConstAtom.
std::set<VarAtom *> ConstraintsEnv::resetSolution(VarAtomPred Pred, ConstAtom *C) {
  std::set<VarAtom *> Unchanged;
  for (auto &CurrE : environment) {
    VarAtom *VA = CurrE.first;
    if (Pred(VA)) {
      if (useChecked) {
        CurrE.second.first = C;
      } else {
        CurrE.second.second = C;
      }
    } else {
      Unchanged.insert(VA);
    }
  }
  return Unchanged;
}

void ConstraintsEnv::resetFullSolution(VarSolTy InitC) {
  for (auto &CurrE : environment) {
    CurrE.second = InitC;
  }
}

// Copy solutions from the ptyp map into the checked one
//   if the checked solution is non-WILD.
void ConstraintsEnv::mergePtrTypes() {
  useChecked = true;
  for (auto &Elem : environment) {
    VarAtom *VA = dyn_cast<VarAtom>(Elem.first);
    ConstAtom *CAssign = Elem.second.first;
    if (dyn_cast<WildAtom>(CAssign) == nullptr) {
      ConstAtom *OAssign = Elem.second.second;
      assert(dyn_cast<WildAtom>(OAssign) == nullptr &&
          "Expected a checked pointer type.");
      assign(VA, OAssign);
    }
  }
}
