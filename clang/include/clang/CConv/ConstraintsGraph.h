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
#include "clang/CConv/Constraints.h"

using namespace boost;
using namespace std;

template<class G>
class BaseGraph {
public:
  typedef typename boost::graph_traits<G>::vertex_descriptor vertex_t;
  typedef std::map<Atom*, vertex_t> VertexMapType;
  G CG;
protected:
  VertexMapType AtomToVDMap;

  virtual vertex_t addVertex(Atom *A) {
    if (AtomToVDMap.find(A) == AtomToVDMap.end()) {
      auto Vidx = add_vertex(A, CG);
      AtomToVDMap[A] = Vidx;
    }
    return AtomToVDMap[A];
  }
};

class ConstraintsGraph
    : public BaseGraph<adjacency_list<setS, vecS, bidirectionalS, Atom *>> {
public:
  typedef adjacency_list<setS, vecS, bidirectionalS, Atom*> DirectedGraphType;

  ConstraintsGraph() {
    AllConstAtoms.clear();
    AtomToVDMap.clear();
  }

  void addConstraint(Geq *C, const Constraints &CS);


  // Get all ConstAtoms, basically the points
  // from where the constraint solving should begin.
  std::set<ConstAtom*> &getAllConstAtoms();
  void forEachEdge(llvm::function_ref<void(Atom*,Atom*)>) const;

  // Get all successors of a given Atom which are of particular type.
  template <typename ConstraintType>
  bool getNeighbors(Atom *A, std::set<Atom*> &Atoms, bool Succs) {
    // Get the vertex descriptor.
    auto Vidx = addVertex(A);
    Atoms.clear();
    if (Succs) {
      typename graph_traits<DirectedGraphType>::out_edge_iterator ei, ei_end;
      for (boost::tie(ei, ei_end) = out_edges(Vidx, CG); ei != ei_end; ++ei) {
        auto source = boost::source(*ei, CG);
        auto target = boost::target(*ei, CG);
        assert(CG[source] == A && "Source has to be the given node.");
        if (clang::dyn_cast<ConstraintType>(CG[target])) {
          Atoms.insert(CG[target]);
        }
      }
    } else {
      typename graph_traits <DirectedGraphType>::in_edge_iterator ei, ei_end;
      for (boost::tie(ei, ei_end) = in_edges(Vidx, CG); ei != ei_end; ++ei) {
        auto source = boost::source ( *ei, CG );
        auto target = boost::target ( *ei, CG );
        assert(CG[target] == A && "Target has to be the given node.");
        if (clang::dyn_cast<ConstraintType>(CG[source])) {
          Atoms.insert(CG[source]);
        }
      }
    }
    return !Atoms.empty();
  }

  vertex_t addVertex(Atom *A);

private:
  std::set<ConstAtom*> AllConstAtoms;
};

// Used during debugging to create a single graph that contains edges and nodes
// from all constraint graphs. This single graph can then be printed to a file
// in graphviz format.
enum EdgeType { Checked, Ptype };
class EdgeProperties {
public:
  EdgeProperties(EdgeType Type, bool IsBidirectional);
  EdgeType Type;
  bool IsBidirectional;
};

class GraphVizOutputGraph
    : public BaseGraph<
          adjacency_list<vecS, vecS, bidirectionalS, Atom *, EdgeProperties *>> {
public:
  typedef adjacency_list<vecS, vecS, bidirectionalS, Atom *, EdgeProperties *>
      DirectedGraphType;

  void mergeConstraintGraph(const ConstraintsGraph& Graph, EdgeType EdgeType);

  // Dump the graph to stdout in a dot format.
  void dumpCGDot(const std::string& GraphDotFile);

  static void dumpConstraintGraphs(const std::string &GraphDotFile,
                                   const ConstraintsGraph& Chk,
                                   const ConstraintsGraph& Pty);

private:
  const std::string EdgeTypeColors[2] = { "red", "blue" };
  const std::string EdgeDirections[2] = { "forward", "both" };
};

#endif // _CONSTRAINTSGRAPH_H
