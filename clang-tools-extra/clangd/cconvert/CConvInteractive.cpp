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
                                     std::set<std::string> &SourceFiles);

DisjointSet& CConvInterface::GetWILDPtrsInfo() {
  return GlobalProgramInfo.getPointerConstraintDisjointSet();
}

void CConvInterface::ResetAllPointerConstraints() {
  // Restore all the deleted constraints.
  Constraints::EnvironmentMap &EnvMap =
      GlobalProgramInfo.getConstraints().getVariables();
  for (auto &CV : EnvMap) {
    CV.first->resetErasedConstraints();
  }
}

bool CConvInterface::MakeSinglePtrNonWild(ConstraintKey targetPtr) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
  CVars RemovePtrs;
  RemovePtrs.clear();

  auto &PtrDisjointSet = GlobalProgramInfo.getPointerConstraintDisjointSet();
  auto &CS = GlobalProgramInfo.getConstraints();

  // Get all the current WILD pointers.
  CVars OldWildPtrs = PtrDisjointSet.AllWildPtrs;

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
  performIterativeItypeRefinement(CS, GlobalProgramInfo, FilePaths);

  // Compute new disjoint set.
  GlobalProgramInfo.computePointerDisjointSet();

  // Get new WILD pointers.
  CVars &NewWildPtrs = PtrDisjointSet.AllWildPtrs;

  // Get the number of pointers that have now converted to non-WILD.
  std::set_difference(OldWildPtrs.begin(), OldWildPtrs.end(),
                      NewWildPtrs.begin(), NewWildPtrs.end(),
                      std::inserter(RemovePtrs, RemovePtrs.begin()));

  return !RemovePtrs.empty();
}


void CConvInterface::InvalidateAllConstraintsWithReason(
                     Constraint *ConstraintToRemove) {
  // Get the reason for the current constraint.
  std::string ConstraintRsn = ConstraintToRemove->getReason();
  Constraints::ConstraintSet ToRemoveConstraints;
  Constraints &CS = GlobalProgramInfo.getConstraints();
  // Remove all constraints that have the reason.
  CS.removeAllConstraintsOnReason(ConstraintRsn, ToRemoveConstraints);

  // Free up memory by deleting all the removed constraints.
  for (auto *toDelCons: ToRemoveConstraints) {
    assert(dyn_cast<Eq>(toDelCons) && "We can only delete Eq constraints.");
    Eq*TCons = dyn_cast<Eq>(toDelCons);
    auto *Vatom = dyn_cast<VarAtom>(TCons->getLHS());
    assert(Vatom != nullptr && "Equality constraint with out VarAtom as LHS");
    VarAtom *VS = CS.getOrCreateVar(Vatom->getLoc());
    VS->getAllConstraints().erase(TCons);
    delete (toDelCons);
  }
}

bool CConvInterface::InvalidateWildReasonGlobally(ConstraintKey PtrKey) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  CVars RemovePtrs;
  RemovePtrs.clear();

  auto &PtrDisjointSet = GlobalProgramInfo.getPointerConstraintDisjointSet();
  auto &CS = GlobalProgramInfo.getConstraints();

  CVars OldWildPtrs = PtrDisjointSet.AllWildPtrs;

  ResetAllPointerConstraints();

  // Delete ALL the constraints that have the same given reason.
  VarAtom *VA = CS.getOrCreateVar(PtrKey);
  Eq NewE(VA, CS.getWild());
  Constraint *OriginalConstraint = *CS.getConstraints().find(&NewE);
  InvalidateAllConstraintsWithReason(OriginalConstraint);

  // Reset constraint solver.
  CS.resetConstraints();

  // Solve the constraint.
  performIterativeItypeRefinement(CS, GlobalProgramInfo, FilePaths);

  // Recompute the WILD pointer disjoint sets.
  GlobalProgramInfo.computePointerDisjointSet();

  // Computed the number of removed pointers.
  CVars &NewWildPtrs = PtrDisjointSet.AllWildPtrs;

  std::set_difference(OldWildPtrs.begin(), OldWildPtrs.end(),
                      NewWildPtrs.begin(), NewWildPtrs.end(),
                      std::inserter(RemovePtrs, RemovePtrs.begin()));

  return !RemovePtrs.empty();
}