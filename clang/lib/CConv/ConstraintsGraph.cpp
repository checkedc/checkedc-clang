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
#include <boost/graph/graphviz.hpp>

// This has to be included to avoid linking errors with boost libraries.
#define BOOST_NO_EXCEPTIONS
#include <boost/throw_exception.hpp>
#include <llvm/Support/raw_ostream.h>
void boost::throw_exception(std::exception const & e) {
//do nothing
}

ConstraintsGraph::vertex_t ConstraintsGraph::addVertex(Atom *A) {
  // Save all the const atoms.
  if (ConstAtom *CA = clang::dyn_cast<ConstAtom>(A)) {
    AllConstAtoms.insert(CA);
  }
  return BaseGraph::addVertex(A);
}

std::set<ConstAtom*> &ConstraintsGraph::getAllConstAtoms() {
  return AllConstAtoms;
}

void
ConstraintsGraph::forEachEdge(llvm::function_ref<void(Atom*,Atom*)> fn) const {
  auto EI = boost::edges(CG);
  for (auto E = EI.first; E != EI.second; ++E) {
    auto s = source(*E, CG);
    auto t = target(*E, CG);
    fn(CG[s],CG[t]);
  }
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
  auto V1 = addVertex(A1);
  auto V2 = addVertex(A2);
  add_edge(V2, V1, CG);
}

void GraphVizOutputGraph::mergeConstraintGraph(const ConstraintsGraph& Graph,
                                               EdgeType EdgeType) {
  Graph.forEachEdge( [this, EdgeType] (Atom* S, Atom* T) {
    auto SVertex = addVertex(S);
    auto TVertex = addVertex(T);

    // If an edge of the same type exists oriented the other direction, update
    // the properties of that edge to indicate that it should be drawn as
    // bidirectional.
    auto OldEdge = edge(TVertex, SVertex, CG);
    if (OldEdge.second) {
      EdgeProperties *OldProps =  CG[OldEdge.first];
      if(OldProps->Type == EdgeType) {
        OldProps->IsBidirectional = true;
        return;
      }
    }

    // Otherwise, create a new edge that is not bidirectional.
    EdgeProperties *EProps = new EdgeProperties(EdgeType, false);
    add_edge(SVertex, TVertex, EProps, CG);
  });
}

void GraphVizOutputGraph::dumpCGDot(const std::string& GraphDotFile) {
   std::ofstream DotFile;
   DotFile.open(GraphDotFile);
   write_graphviz(DotFile, CG,
     [&] (std::ostream &out, unsigned v) {
       out << "[label=\"" << CG[v]->getStr() << "\"]";
     },
     [&] (std::ostream &out,
              boost::detail::edge_desc_impl<boost::bidirectional_tag,
                                            long unsigned int> e)  {
       EdgeProperties *EProps = CG[e];
       std::string Color = EdgeTypeColors[EProps->Type];
       std::string Dir = EdgeDirections[EProps->IsBidirectional];
       out << "[color=\"" << Color << "\" " << "dir=\"" << Dir <<"\"]";
     });
   DotFile.close();
}

void GraphVizOutputGraph::dumpConstraintGraphs(const std::string &GraphDotFile,
                                               const ConstraintsGraph& Chk,
                                               const ConstraintsGraph& Pty) {
  GraphVizOutputGraph OutGraph;
  OutGraph.mergeConstraintGraph(Chk, Checked);
  OutGraph.mergeConstraintGraph(Pty,Ptype);
  OutGraph.dumpCGDot(GraphDotFile);
}

EdgeProperties::EdgeProperties(EdgeType Type, bool IsBidirectional)
    : Type(Type), IsBidirectional(IsBidirectional) {}
