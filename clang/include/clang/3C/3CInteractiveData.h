//=--3CInteractiveData.h------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data structures used to communicate results of 3C interactive mode
//
//===----------------------------------------------------------------------===//

#ifndef _3CINTERACTIVEDATA_H
#define _3CINTERACTIVEDATA_H

#include "ConstraintVariables.h"
#include "PersistentSourceLoc.h"

// Source info and reason for each wild pointer.
class WildPointerInferenceInfo {
public:
  WildPointerInferenceInfo(std::string Reason, const PersistentSourceLoc PSL)
      : WildPtrReason(Reason), Location(PSL) {}

  const std::string &getWildPtrReason() const { return WildPtrReason; }
  const PersistentSourceLoc &getLocation() const { return Location; }

private:
  std::string WildPtrReason = "";
  PersistentSourceLoc Location;
};

// Constraints information.
class ConstraintsInfo {
  friend class ProgramInfo;

public:
  ConstraintsInfo() {}
  void Clear();
  CVars &GetRCVars(ConstraintKey);
  CVars &GetSrcCVars(ConstraintKey);
  CVars getWildAffectedCKeys(const std::set<ConstraintKey> &DWKeys);
  void printStats(llvm::raw_ostream &O);
  void printRootCauseStats(raw_ostream &O, Constraints &CS);
  int getNumPtrsAffected(ConstraintKey CK);

  std::map<ConstraintKey, WildPointerInferenceInfo> RootWildAtomsWithReason;
  CVars AllWildAtoms;
  CVars InSrcWildAtoms;
  CVars TotalNonDirectWildAtoms;
  CVars InSrcNonDirectWildAtoms;
  std::set<std::string> ValidSourceFiles;
  std::map<ConstraintKey, const PersistentSourceLoc *> AtomSourceMap;

private:
  // Root cause map: This is the map of a Constraint var and a set of
  // Constraint vars (that are directly assigned WILD) which are the reason
  // for making the above constraint var WILD.
  // Example:
  //  WILD
  //  / \
  // p   q
  // \    \
  //  \    r
  //   \  /
  //    s
  // Here: s -> {p, q} and r -> {q}
  std::map<ConstraintKey, CVars> RCMap;
  // This is source map: Map of Constraint var (which are directly
  // assigned WILD) and the set of constraint vars which are WILD because of
  // the above constraint.
  // For the above case, this contains: p -> {s}, q -> {r, s}
  std::map<ConstraintKey, CVars> SrcWMap;

  std::map<ConstraintVariable *, CVars> PtrRCMap;
  std::map<ConstraintKey, std::set<ConstraintVariable *>> PtrSrcWMap;

  // Get score for each of the ConstraintKeys, which are wild.
  // For the above example, the score of s would be 0.5, similarly
  // the score of r would be 1
  float getAtomAffectedScore(const CVars &AllKeys);

  float getPtrAffectedScore(const std::set<ConstraintVariable *> CVs);

  void printConstraintStats(raw_ostream &O, Constraints &CS,
                            ConstraintKey Cause);
};

#endif // _3CINTERACTIVEDATA_H
