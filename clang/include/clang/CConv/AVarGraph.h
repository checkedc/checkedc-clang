//=--AVarGraph.h--------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Class that maintains the graph between all the variables that are relevant
// to array bounds inference.
//
//===----------------------------------------------------------------------===//

#ifndef _AVARGRAPH_H
#define _AVARGRAPH_H

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include "ProgramVar.h"

using namespace boost;
using namespace std;
class AVarBoundsInfo;

template<class G>
class AVarBaseGraph {
public:
  typedef typename boost::graph_traits<G>::vertex_descriptor vertex_t;
  typedef std::map<BoundsKey, vertex_t> VertexMapType;
  G CG;

  virtual vertex_t addVertex(BoundsKey BK) {
    if (BkeyToVDMap.find(BK) == BkeyToVDMap.end()) {
      auto Vidx = add_vertex(BK, CG);
      BkeyToVDMap[BK] = Vidx;
    }
    return BkeyToVDMap[BK];
  }

protected:
  VertexMapType BkeyToVDMap;
};

// Graph that keeps tracks of direct assignments between various variables.
class AVarGraph
    : public AVarBaseGraph<adjacency_list<setS, vecS,
                                          bidirectionalS, BoundsKey>> {
public:
  typedef adjacency_list<setS, vecS,
                         bidirectionalS, BoundsKey> DirectedGraphType;

  AVarGraph() {
    clear();
  }

  void clear() {
    BkeyToVDMap.clear();
    CG.clear();
  }

  // Get all predecessors of the given bounds key K
  bool getPredecessors(BoundsKey K, std::set<BoundsKey> &Pred);
  bool getSuccessors(BoundsKey K, std::set<BoundsKey> &Succ);

  // Add edge between two bounds key. The flag BD indicates if the edge
  // is bidirectional.
  void addEdge(BoundsKey L, BoundsKey R, bool BD = true);

  void dumpCGDot(const std::string &GraphDotFile, AVarBoundsInfo *ABInfo);
};

#endif // _AVARGRAPH_H
