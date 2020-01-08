//
// Created by machiry on 11/11/19.
//

#include "CConvInteractive.h"

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