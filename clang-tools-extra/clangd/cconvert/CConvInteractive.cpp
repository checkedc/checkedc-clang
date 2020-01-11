//
// Created by machiry on 11/11/19.
//

#include "CConvInteractive.h"
#include "ProgramInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

bool performIterativeItypeRefinement(Constraints &CS, ProgramInfo &Info, std::set<std::string> &inputSourceFiles);

void DisjointSet::clear() {
  leaders.clear();
  groups.clear();
  realWildPtrsWithReasons.clear();
  PtrSourceMap.clear();
  allWildPtrs.clear();
  totalNonDirectWildPointers.clear();
  validSourceFiles.clear();
}
void DisjointSet::addElements(ConstraintKey a, ConstraintKey b) {
  if (leaders.find(a) != leaders.end()) {
    if (leaders.find(b) != leaders.end()) {
      auto leadera = leaders[a];
      auto leaderb = leaders[b];
      auto &grpa = groups[leadera];
      auto &grpb = groups[leaderb];

      if (grpa.size() < grpb.size()) {
        grpa = groups[leaderb];
        grpb = groups[leadera];
        leadera = leaders[b];
        leaderb = leaders[a];
      }
      grpa.insert(grpb.begin(), grpb.end());
      groups.erase(leaderb);
      for (auto k: grpb) {
        leaders[k] = leadera;
      }

    } else {
      groups[leaders[a]].insert(b);
      leaders[b] = leaders[a];
    }
  } else {
    if (leaders.find(b) != leaders.end()) {
      groups[leaders[b]].insert(a);
      leaders[a] = leaders[b];
    } else {
      leaders[a] = leaders[b] = a;
      groups[a].insert(a);
      groups[a].insert(b);
    }
  }
}

DisjointSet& getWILDPtrsInfo() {
  return GlobalProgInfo.getPointerConstraintDisjointSet();
}

static void resetAllPointerConstraints() {
  Constraints::EnvironmentMap &currEnvMap = GlobalProgInfo.getConstraints().getVariables();
  for (auto &CV : currEnvMap) {
    CV.first->resetErasedConstraints();
  }
}

bool makeSinglePtrNonWild(ConstraintKey targetPtr) {

  CVars removePtrs;
  removePtrs.clear();
  CVars oldWILDPtrs = GlobalProgInfo.getPointerConstraintDisjointSet().allWildPtrs;

  resetAllPointerConstraints();

  errs() << "After resetting\n";

  VarAtom *VA = GlobalProgInfo.getConstraints().getOrCreateVar(targetPtr);

  Eq newE(VA, GlobalProgInfo.getConstraints().getWild());

  Constraint *originalConstraint = *GlobalProgInfo.getConstraints().getConstraints().find(&newE);

  GlobalProgInfo.getConstraints().removeConstraint(originalConstraint);
  VA->getAllConstraints().erase(originalConstraint);

  delete(originalConstraint);

  GlobalProgInfo.getConstraints().resetConstraints();

  performIterativeItypeRefinement(GlobalProgInfo.getConstraints(), GlobalProgInfo, inputFilePaths);
  GlobalProgInfo.computePointerDisjointSet();

  CVars &newWILDPtrs = GlobalProgInfo.getPointerConstraintDisjointSet().allWildPtrs;

  std::set_difference(oldWILDPtrs.begin(), oldWILDPtrs.end(),
                      newWILDPtrs.begin(),
                      newWILDPtrs.end(),
                      std::inserter(removePtrs, removePtrs.begin()));
  errs() << "Returning\n";

  return !removePtrs.empty();
}


static void invalidateAllConstraintsWithReason(Constraint *constraintToRemove) {
  std::string constraintReason = constraintToRemove->getReason();
  Constraints::ConstraintSet toRemoveConstraints;
  Constraints &CS = GlobalProgInfo.getConstraints();
  CS.removeAllConstraintsBasedOnThisReason(constraintReason, toRemoveConstraints);

  for (auto *toDelCons: toRemoveConstraints) {
    assert(dyn_cast<Eq>(toDelCons) && "We can only delete Eq constraints.");
    Eq* tCons = dyn_cast<Eq>(toDelCons);
    VarAtom *VS = CS.getOrCreateVar((dyn_cast<VarAtom>(tCons->getLHS()))->getLoc());
    VS->getAllConstraints().erase(tCons);
    // free the memory.
    delete (toDelCons);
  }
}

bool invalidateWildReasonGlobally(ConstraintKey targetPtr) {

  CVars removePtrs;
  removePtrs.clear();
  CVars oldWILDPtrs = GlobalProgInfo.getPointerConstraintDisjointSet().allWildPtrs;

  resetAllPointerConstraints();

  errs() << "After resetting\n";

  VarAtom *VA = GlobalProgInfo.getConstraints().getOrCreateVar(targetPtr);

  Eq newE(VA, GlobalProgInfo.getConstraints().getWild());

  Constraint *originalConstraint = *GlobalProgInfo.getConstraints().getConstraints().find(&newE);

  invalidateAllConstraintsWithReason(originalConstraint);

  GlobalProgInfo.getConstraints().resetConstraints();

  performIterativeItypeRefinement(GlobalProgInfo.getConstraints(), GlobalProgInfo, inputFilePaths);

  GlobalProgInfo.computePointerDisjointSet();

  CVars &newWILDPtrs = GlobalProgInfo.getPointerConstraintDisjointSet().allWildPtrs;

  std::set_difference(oldWILDPtrs.begin(), oldWILDPtrs.end(),
                      newWILDPtrs.begin(),
                      newWILDPtrs.end(),
                      std::inserter(removePtrs, removePtrs.begin()));

  errs() << "Returning\n";

  return !removePtrs.empty();
}