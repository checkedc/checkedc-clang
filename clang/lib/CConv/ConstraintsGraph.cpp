//=--ConstraintsGraph.cpp-----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of methods in ConstraintsGraph.cpp
//
//===----------------------------------------------------------------------===//

#include "clang/CConv/ConstraintsGraph.h"
#include <iostream>
#include <llvm/Support/raw_ostream.h>

CGNode<ConstraintEdge> *ConstraintsGraph::addVertex(Atom *A) {
  // Save all the const atoms.
  if (ConstAtom *CA = clang::dyn_cast<ConstAtom>(A))
    AllConstAtoms.insert(CA);
  auto *N = new CGNode<ConstraintEdge>(A);
  auto *OldN = findNode(*N);
  if (OldN != end()) {
    delete N;
    return *OldN;
  }
  addNode(*N);
  return N;
}

std::set<ConstAtom*> &ConstraintsGraph::getAllConstAtoms() {
  return AllConstAtoms;
}

void
ConstraintsGraph::forEachEdge(llvm::function_ref<void(Atom*,Atom*)> fn) const {
  for (auto *N : Nodes)
    for (auto *E : N->getEdges())
      fn(N->getAtom(), E->getTargetNode().getAtom());
}

void ConstraintsGraph::addConstraint(Geq *C, const Constraints &CS) {
  Atom *A1 = C->getLHS();
  if (VarAtom *VA1 = clang::dyn_cast<VarAtom>(A1)) {
    assert(CS.getVar(VA1->getLoc()) == VA1);
  }
  Atom *A2 = C->getRHS();
  if (VarAtom *VA2 = clang::dyn_cast<VarAtom>(A2)) {
    assert(CS.getVar(VA2->getLoc()) == VA2);
  }
  CGNode<ConstraintEdge> *V1 = addVertex(A1);
  CGNode<ConstraintEdge> *V2 = addVertex(A2);
  ConstraintEdge *E = new ConstraintEdge(*V1);
  connect(*V2, *V1, *E);
}

void ConstraintsGraph::removeEdge(Atom *Src, Atom *Dst) {
  auto *NSrc = findNode(CGNode<ConstraintEdge>(Src));
  auto *NDst = findNode(CGNode<ConstraintEdge>(Dst));
  assert(NSrc != end() && NDst != end());
  EdgeListTy Edges;
  (*NDst)->findEdgesTo(**NSrc, Edges);
  for (ConstraintEdge *E : Edges) {
    (*NDst)->removeEdge(*E);
    delete E;
  }
}

ConstraintsGraph::~ConstraintsGraph() {
  for (auto *N : Nodes) {
    for (auto *E : N->getEdges())
      delete E;
    N->getEdges().clear();
    delete N;
    N = nullptr;
  }
  Nodes.clear();
}

//void GraphVizOutputGraph::mergeConstraintGraph(const ConstraintsGraph &Graph,
//                                               EdgeType EdgeType) {
//  Graph.forEachEdge( [this, EdgeType] (Atom* S, Atom* T) {
//    auto SVertex = addVertex(S);
//    auto TVertex = addVertex(T);
//
//    // If an edge of the same type exists oriented the other direction, update
//    // the properties of that edge to indicate that it should be drawn as
//    // bidirectional.
//    auto OldEdge = edge(TVertex, SVertex, CG);
//    if (OldEdge.second) {
//      EdgeProperties *OldProps =  CG[OldEdge.first];
//      if(OldProps->Type == EdgeType) {
//        OldProps->IsBidirectional = true;
//        return;
//      }
//    }
//
//    // Otherwise, create a new edge that is not bidirectional.
//    EdgeProperties *EProps = new EdgeProperties(EdgeType, false);
//    add_edge(SVertex, TVertex, EProps, CG);
//  });
//}
//
//void GraphVizOutputGraph::dumpCGDot(const std::string &GraphDotFile) {
//   std::ofstream DotFile;
//   DotFile.open(GraphDotFile);
//   write_graphviz(DotFile, CG,
//     [&] (std::ostream &out, unsigned v) {
//       out << "[label=\"" << CG[v]->getStr() << "\"]";
//     },
//     [&] (std::ostream &out,
//              boost::detail::edge_desc_impl<boost::bidirectional_tag,
//                                            long unsigned int> e)  {
//       EdgeProperties *EProps = CG[e];
//       std::string Color = EdgeTypeColors[EProps->Type];
//       std::string Dir = EdgeDirections[EProps->IsBidirectional];
//       out << "[color=\"" << Color << "\" " << "dir=\"" << Dir <<"\"]";
//     });
//   DotFile.close();
//}
//
//void GraphVizOutputGraph::dumpConstraintGraphs(const std::string &GraphDotFile,
//                                               const ConstraintsGraph &Chk,
//                                               const ConstraintsGraph &Pty) {
//  GraphVizOutputGraph OutGraph;
//  OutGraph.mergeConstraintGraph(Chk, Checked);
//  OutGraph.mergeConstraintGraph(Pty,Ptype);
//  OutGraph.dumpCGDot(GraphDotFile);
//}
//
//EdgeProperties::EdgeProperties(EdgeType Type, bool IsBidirectional)
//    : Type(Type), IsBidirectional(IsBidirectional) {}
//