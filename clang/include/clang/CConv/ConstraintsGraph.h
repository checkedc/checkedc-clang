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

#include <queue>
#include "clang/CConv/Constraints.h"
#include "llvm/ADT/DirectedGraph.h"

#include <queue>
#include "clang/CConv/Constraints.h"
#include "llvm/ADT/DirectedGraph.h"

template<class EdgeType>
class CGNode : public llvm::DGNode<CGNode<EdgeType>, EdgeType> {
public:
  CGNode() = delete;
  CGNode(Atom *A, EdgeType &E) : llvm::DGNode<CGNode<EdgeType>, EdgeType>(E),
                                 A(A) {}
  CGNode(Atom *A) : llvm::DGNode<CGNode<EdgeType>, EdgeType>(), A(A) {}
  CGNode(const CGNode &N) : llvm::DGNode<CGNode<EdgeType>, EdgeType>(N),
                            A(N.A) {}
  CGNode(const CGNode &&N)
      : llvm::DGNode<CGNode<EdgeType>, EdgeType>(std::move(N)), A(N.A) {}

  ~CGNode() {}

  CGNode &operator=(const CGNode &N) {
    CGNode::operator=(N);
    A = N.A;
    return *this;
  }

  CGNode &operator=(CGNode &&N) {
    CGNode::operator=(std::move(N));
    A = N.A;
    return *this;
  }

  Atom *getAtom() const { return A; }

  bool isEqualTo(const CGNode<EdgeType> &N) const { return this->A == N.A; }

private:
  Atom *A;

};

struct ConstraintEdge : llvm::DGEdge<CGNode<ConstraintEdge>, ConstraintEdge> {
  explicit ConstraintEdge(CGNode<ConstraintEdge> &Node) : DGEdge(Node) {}
  explicit ConstraintEdge(const ConstraintEdge &E) : DGEdge(E) {}

  bool isEqualTo(const ConstraintEdge &E) const {
    return this->getTargetNode() == E.getTargetNode();
  }
};

class ConstraintsGraph : public llvm::DirectedGraph<CGNode<ConstraintEdge>,
                                                    ConstraintEdge> {
public:
  ~ConstraintsGraph();

  void addConstraint(Geq *C, const Constraints &CS);

  std::set<ConstAtom*> &getAllConstAtoms();
  void forEachEdge(llvm::function_ref<void(Atom*,Atom*)>) const;
  void removeEdge(Atom *Src, Atom *Dst);

  // Get all successors of a given Atom which are of particular type.
  template <typename AtomType>
  bool getNeighbors(Atom *A, std::set<AtomType*> &Atoms, bool Succs) {
    auto *N = findNode(CGNode<ConstraintEdge>(A));
    if (N == end())
      return false;
    Atoms.clear();
    if (Succs) {
      const auto ES = (*N)->getEdges();
      for (auto *E : ES) {
        Atom *NAtom = E->getTargetNode().getAtom();
        if (auto *AType = clang::dyn_cast<AtomType>(NAtom))
          Atoms.insert(AType);
      }
    } else {
      for (auto *Neighbor : Nodes) {
        if (Neighbor == *N)
          continue;
        if (Neighbor->hasEdgeTo(**N)) {
          Atom *NAtom = Neighbor->getAtom();
          if (auto *AType = clang::dyn_cast<AtomType>(NAtom))
            Atoms.insert(AType);
        }
      }
    }
    return !Atoms.empty();
  }

  template <typename AtomType>
  void breadthFirstSearch(Atom *start, llvm::function_ref<void(AtomType*)> Fn) {
    std::queue<CGNode<ConstraintEdge>*> SearchQueue;
    std::set<CGNode<ConstraintEdge>*> VisitedSet;
    auto *N = findNode(CGNode<ConstraintEdge>(start));
    if (N == end())
      return;
    SearchQueue.push(*N);
    while (!SearchQueue.empty()) {
      CGNode<ConstraintEdge> *Node = SearchQueue.front();
      SearchQueue.pop();
      if (VisitedSet.find(Node) != VisitedSet.end())
        continue;
      VisitedSet.insert(Node);
      Atom *A = Node->getAtom();
      if (auto *C = llvm::dyn_cast<AtomType>(A))
        Fn(C);
      for (auto *E : Node->getEdges())
        SearchQueue.push(&E->getTargetNode());
    }
  }

private:
  std::set<ConstAtom*> AllConstAtoms;

  CGNode<ConstraintEdge> *addVertex(Atom *A);
};

// Used during debugging to create a single graph that contains edges and nodes
// from all constraint graphs. This single graph can then be printed to a file
// in graphviz format.
//enum EdgeType { Checked, Ptype };
//class EdgeProperties {
//public:
//  EdgeProperties(EdgeType Type, bool IsBidirectional);
//  EdgeType Type;
//  bool IsBidirectional;
//};
//
//class GraphVizOutputGraph
//    : public BaseGraph<
//        boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
//                                Atom *, EdgeProperties *>> {
//public:
//  typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
//                                Atom *, EdgeProperties *>
//      DirectedGraphType;
//
//  void mergeConstraintGraph(const ConstraintsGraph &Graph, EdgeType EdgeType);
//
//  // Dump the graph to stdout in a dot format.
//  void dumpCGDot(const std::string& GraphDotFile);
//
//  static void dumpConstraintGraphs(const std::string &GraphDotFile,
//                                   const ConstraintsGraph &Chk,
//                                   const ConstraintsGraph &Pty);
//
//private:
//  const std::string EdgeTypeColors[2] = { "red", "blue" };
//  const std::string EdgeDirections[2] = { "forward", "both" };
//};

#endif // _CONSTRAINTSGRAPH_H
