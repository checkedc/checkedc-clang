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

#include "clang/CConv/CConvInteractiveData.h"

void DisjointSet::Clear() {
  Leaders.clear();
  Groups.clear();
  RealWildPtrsWithReasons.clear();
  PtrSourceMap.clear();
  AllWildPtrs.clear();
  TotalNonDirectWildPointers.clear();
  ValidSourceFiles.clear();
}
void DisjointSet::AddElements(ConstraintKey A, ConstraintKey B) {
  if (Leaders.find(A) != Leaders.end()) {
    if (Leaders.find(B) != Leaders.end()) {
      auto LeaderA = Leaders[A];
      auto LeaderB = Leaders[B];
      auto &GrpA = Groups[LeaderA];
      auto &GrpB = Groups[LeaderB];

      if (GrpA.size() < GrpB.size()) {
        GrpA = Groups[LeaderB];
        GrpB = Groups[LeaderA];
        LeaderA = Leaders[B];
        LeaderB = Leaders[A];
      }
      GrpA.insert(GrpB.begin(), GrpB.end());
      for (auto k : GrpB) {
        Leaders[k] = LeaderA;
      }
      Groups.erase(LeaderB);
    } else {
      Groups[Leaders[A]].insert(B);
      Leaders[B] = Leaders[A];
    }
  } else {
    if (Leaders.find(B) != Leaders.end()) {
      Groups[Leaders[B]].insert(A);
      Leaders[A] = Leaders[B];
    } else {
      Leaders[A] = Leaders[B] = A;
      Groups[A].insert(A);
      Groups[A].insert(B);
    }
  }
}

ConstraintKey DisjointSet::GetLeader(ConstraintKey Ckey) {
  return Leaders[Ckey];
}

CVars &DisjointSet::GetGroup(ConstraintKey Ckey) {
  return Groups[Ckey];
}
