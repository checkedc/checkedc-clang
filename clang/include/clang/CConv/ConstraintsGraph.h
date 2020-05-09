//=--ConstraintsGraph.h-------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Class we use to maintain constraints in a graph form.
//
//===----------------------------------------------------------------------===//

#ifndef _CONSTRAINTSGRAPH_H
#define _CONSTRAINTSGRAPH_H

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include "clang/CConv/Constraints.h"

using namespace boost;
using namespace std;

class ConstraintsGraph {
public:
  typedef adjacency_list<setS, vecS, directedS, Atom*> DirectedGraphType;
  typedef boost::graph_traits<DirectedGraphType>::vertex_descriptor vertex_t;
  typedef std::map<Atom*, vertex_t> VertexMapType;

  ConstraintsGraph() {
    AllConstAtoms.clear();
    AtomToVDMap.clear();
  }

  void addEdge(Atom *V1, Atom *V2, bool isBackward);
  void addConstraint(Geq *C, Constraints &CS);

  // Get all ConstAtoms, basically the points
  // from where the constraint solving should begin.
  std::set<ConstAtom*> &getAllConstAtoms();

  // Get all successors of a given Atom which are of particular type.
  template <typename ConstraintType>
  bool getSuccessors(Atom *CA, std::set<Atom*> &Suc) {
    // Get the vertex descriptor.
    auto Vidx = addVertex(CA);
    Suc.clear();
    typename graph_traits <DirectedGraphType>::out_edge_iterator ei, ei_end;
    for (boost::tie(ei, ei_end) = out_edges(Vidx, CG); ei != ei_end; ++ei) {
      auto source = boost::source ( *ei, CG );
      auto target = boost::target ( *ei, CG );
      assert(CG[source] == CA && "Source has to be the given node.");
      if (clang::dyn_cast<ConstraintType>(CG[target])) {
        Suc.insert(CG[target]);
      }
    }
    return !Suc.empty();
  }
  // Dump the graph to stdout in a dot format.
  void dumpCGDot(const std::string& GraphDotFile);

private:
  std::set<ConstAtom*> AllConstAtoms;
  VertexMapType AtomToVDMap;
  vertex_t addVertex(Atom *A);
  DirectedGraphType CG;
};

#endif // _CONSTRAINTSGRAPH_H
