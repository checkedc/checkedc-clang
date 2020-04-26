//=--CConvInteractiveData.cpp-------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data structure methods
//
//===----------------------------------------------------------------------===//

#include "CConvInteractiveData.h"

void DisjointSet::Clear() {
  Leaders.clear();
  Groups.clear();
  RealWildPtrsWithReasons.clear();
  PtrSourceMap.clear();
  AllWildPtrs.clear();
  TotalNonDirectWildPointers.clear();
  ValidSourceFiles.clear();
}
void DisjointSet::AddElements(ConstraintKey a, ConstraintKey b) {
  if (Leaders.find(a) != Leaders.end()) {
    if (Leaders.find(b) != Leaders.end()) {
      auto leadera = Leaders[a];
      auto leaderb = Leaders[b];
      auto &grpa = Groups[leadera];
      auto &grpb = Groups[leaderb];

      if (grpa.size() < grpb.size()) {
        grpa = Groups[leaderb];
        grpb = Groups[leadera];
        leadera = Leaders[b];
        leaderb = Leaders[a];
      }
      grpa.insert(grpb.begin(), grpb.end());
      Groups.erase(leaderb);
      for (auto k: grpb) {
        Leaders[k] = leadera;
      }

    } else {
      Groups[Leaders[a]].insert(b);
      Leaders[b] = Leaders[a];
    }
  } else {
    if (Leaders.find(b) != Leaders.end()) {
      Groups[Leaders[b]].insert(a);
      Leaders[a] = Leaders[b];
    } else {
      Leaders[a] = Leaders[b] = a;
      Groups[a].insert(a);
      Groups[a].insert(b);
    }
  }
}

ConstraintKey DisjointSet::GetLeader(ConstraintKey ckey) {
  return Leaders[ckey];
}

CVars& DisjointSet::GetGroup(ConstraintKey ckey) {
  return Groups[ckey];
}