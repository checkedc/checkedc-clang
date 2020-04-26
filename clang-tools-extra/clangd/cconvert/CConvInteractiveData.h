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

// Source info and reason for each wild pointer
struct WildPointerInferenceInfo {
  std::string SourceFileName = "";
  std::string WildPtrReason = "";
  bool IsValid = false;
  unsigned LineNo = 0;
  unsigned ColStart = 0;
};

// Standard implementation of disjoint sets
class DisjointSet {
  friend class ProgramInfo;
public:
  DisjointSet() {

  }
  void Clear();
  void AddElements(ConstraintKey, ConstraintKey);
  ConstraintKey GetLeader(ConstraintKey);
  CVars& GetGroup(ConstraintKey);

  std::map<ConstraintKey, struct WildPointerInferenceInfo>
      RealWildPtrsWithReasons;
  CVars AllWildPtrs;
  CVars TotalNonDirectWildPointers;
  std::set<std::string> ValidSourceFiles;
  std::map<ConstraintKey, PersistentSourceLoc*> PtrSourceMap;

private:
  std::map<ConstraintKey, ConstraintKey> Leaders;
  std::map<ConstraintKey, CVars> Groups;
};

#endif // _CCONVINTERACTIVEDATA_H
