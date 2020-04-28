//=--CConvInteractive.cpp-----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of the methods in CConvInteractive.h
//
//===----------------------------------------------------------------------===//

#include "CConvInteractive.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

bool performIterativeItypeRefinement(Constraints &CS, ProgramInfo &Info,
                                     std::set<std::string> &inputSourceFiles);

DisjointSet& CConvInterface::GetWILDPtrsInfo() {
  return GlobalProgramInfo.getPointerConstraintDisjointSet();
}

void CConvInterface::ResetAllPointerConstraints() {
  // Restore all the deleted constraints.
  Constraints::EnvironmentMap &currEnvMap =
      GlobalProgramInfo.getConstraints().getVariables();
  for (auto &CV : currEnvMap) {
    CV.first->resetErasedConstraints();
  }
}

bool CConvInterface::MakeSinglePtrNonWild(ConstraintKey targetPtr) {
  std::lock_guard<std::mutex> lock(InterfaceMutex);
  CVars removePtrs;
  removePtrs.clear();

  auto &ptrDisjointSet = GlobalProgramInfo.getPointerConstraintDisjointSet();
  auto &CS = GlobalProgramInfo.getConstraints();

  // Get all the current WILD pointers.
  CVars oldWILDPtrs = ptrDisjointSet.AllWildPtrs;

  // Reset all the pointer constraints.
  ResetAllPointerConstraints();

  // Delete the constraint that make the provided targetPtr WILD.
  VarAtom *VA = CS.getOrCreateVar(targetPtr);
  Eq newE(VA, CS.getWild());
  Constraint *originalConstraint = *CS.getConstraints().find(&newE);
  CS.removeConstraint(originalConstraint);
  VA->getAllConstraints().erase(originalConstraint);
  delete(originalConstraint);

  // Reset the constraint system.
  CS.resetConstraints();

  // Solve the constraints.
  performIterativeItypeRefinement(CS, GlobalProgramInfo, inputFilePaths);

  // Compute new disjoint set.
  GlobalProgramInfo.computePointerDisjointSet();

  // Get new WILD pointers.
  CVars &newWILDPtrs = ptrDisjointSet.AllWildPtrs;

  // Get the number of pointers that have now converted to non-WILD.
  std::set_difference(oldWILDPtrs.begin(), oldWILDPtrs.end(),
                      newWILDPtrs.begin(),
                      newWILDPtrs.end(),
                      std::inserter(removePtrs, removePtrs.begin()));

  return !removePtrs.empty();
}


void CConvInterface::InvalidateAllConstraintsWithReason(
                     Constraint *constraintToRemove) {
  // Get the reason for the current constraint.
  std::string constraintReason = constraintToRemove->getReason();
  Constraints::ConstraintSet toRemoveConstraints;
  Constraints &CS = GlobalProgramInfo.getConstraints();
  // Remove all constraints that have the reason.
  CS.removeAllConstraintsOnReason(constraintReason,
                                  toRemoveConstraints);

  // Free up memory by deleting all the removed constraints.
  for (auto *toDelCons: toRemoveConstraints) {
    assert(dyn_cast<Eq>(toDelCons) && "We can only delete Eq constraints.");
    Eq* tCons = dyn_cast<Eq>(toDelCons);
    auto *vatom = dyn_cast<VarAtom>(tCons->getLHS());
    assert(vatom != nullptr && "Equality constraint with out VarAtom as LHS");
    VarAtom *VS = CS.getOrCreateVar(vatom->getLoc());
    VS->getAllConstraints().erase(tCons);
    delete (toDelCons);
  }
}

bool CConvInterface::InvalidateWildReasonGlobally(ConstraintKey targetPtr) {
  std::lock_guard<std::mutex> lock(InterfaceMutex);

  CVars removePtrs;
  removePtrs.clear();

  auto &ptrDisjointSet = GlobalProgramInfo.getPointerConstraintDisjointSet();
  auto &CS = GlobalProgramInfo.getConstraints();

  CVars oldWILDPtrs = ptrDisjointSet.AllWildPtrs;

  ResetAllPointerConstraints();

  // Delete ALL the constraints that have the same given reason.
  VarAtom *VA = CS.getOrCreateVar(targetPtr);
  Eq newE(VA, CS.getWild());
  Constraint *originalConstraint = *CS.getConstraints().find(&newE);
  InvalidateAllConstraintsWithReason(originalConstraint);

  // Reset constraint solver.
  CS.resetConstraints();

  // Solve the constraint.
  performIterativeItypeRefinement(CS, GlobalProgramInfo, inputFilePaths);

  // Recompute the WILD pointer disjoint sets.
  GlobalProgramInfo.computePointerDisjointSet();

  // Computed the number of removed pointers.
  CVars &newWILDPtrs = ptrDisjointSet.AllWildPtrs;

  std::set_difference(oldWILDPtrs.begin(), oldWILDPtrs.end(),
                      newWILDPtrs.begin(),
                      newWILDPtrs.end(),
                      std::inserter(removePtrs, removePtrs.begin()));

  return !removePtrs.empty();
}