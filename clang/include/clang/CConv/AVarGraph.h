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

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include "ProgramVar.h"

using namespace boost;
using namespace std;

template<class G>
class AVarBaseGraph {
public:
  typedef typename boost::graph_traits<G>::vertex_descriptor vertex_t;
  typedef std::map<BoundsKey, vertex_t> VertexMapType;

protected:
  G CG;
  VertexMapType BkeyToVDMap;

  virtual vertex_t addVertex(BoundsKey BK) {
    if (BkeyToVDMap.find(BK) == BkeyToVDMap.end()) {
      auto Vidx = add_vertex(BK, CG);
      BkeyToVDMap[BK] = Vidx;
    }
    return BkeyToVDMap[BK];
  }
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

  void addEdge(BoundsKey L, BoundsKey R);
};

#endif // _AVARGRAPH_H
