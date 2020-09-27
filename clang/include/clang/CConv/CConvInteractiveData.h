//=--CConvInteractiveData.h---------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data structures used to communicate results of CConv interactive mode
//
//===----------------------------------------------------------------------===//

#ifndef _CCONVINTERACTIVEDATA_H
#define _CCONVINTERACTIVEDATA_H

#include "ConstraintVariables.h"
#include "PersistentSourceLoc.h"

// Source info and reason for each wild pointer.
struct WildPointerInferenceInfo {
  std::string SourceFileName = "";
  std::string WildPtrReason = "";
  bool IsValid = false;
  unsigned LineNo = 0;
  unsigned ColStartS = 0;
  unsigned ColStartE = 0;
};

// Constraints information.
class ConstraintsInfo {
  friend class ProgramInfo;
public:
  ConstraintsInfo() {

  }
  void Clear();
  CVars &GetRCVars(ConstraintKey);
  CVars &GetSrcCVars(ConstraintKey);
  CVars getWildAffectedCKeys(const std::set<ConstraintKey> &DWKeys);
  void print_stats(llvm::raw_ostream &O);
  void print_per_ptr_stats(llvm::raw_ostream &O, Constraints &CS);

  std::map<ConstraintKey, struct WildPointerInferenceInfo>
      RealWildPtrsWithReasons;
  CVars AllWildPtrs;
  CVars InSrcWildPtrs;
  CVars TotalNonDirectWildPointers;
  CVars InSrcNonDirectWildPointers;
  std::set<std::string> ValidSourceFiles;
  std::map<ConstraintKey, PersistentSourceLoc *> PtrSourceMap;

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

  // Get score for each of the ConstraintKeys, which are wild.
  // For the above example, the score of s would be 0.5, similarly
  // the score of r would be 1
  float getPtrAffectedScore(const CVars &AllKeys);
};

#endif // _CCONVINTERACTIVEDATA_H
