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

ConstraintsGraph::NodeType *ConstraintsGraph::addVertex(Atom *A) {
  // Save all the const atoms.
  if (auto *CA = clang::dyn_cast<ConstAtom>(A))
    AllConstAtoms.insert(CA);
  return DataGraph::addVertex(A);
}

std::set<ConstAtom*> &ConstraintsGraph::getAllConstAtoms() {
  return AllConstAtoms;
}

void ConstraintsGraph::addConstraint(Geq *C, const Constraints &CS) {
  Atom *A1 = C->getLHS();
  if (auto *VA1 = clang::dyn_cast<VarAtom>(A1))
    assert(CS.getVar(VA1->getLoc()) == VA1);

  Atom *A2 = C->getRHS();
  if (auto *VA2 = clang::dyn_cast<VarAtom>(A2))
    assert(CS.getVar(VA2->getLoc()) == VA2);

  addEdge(A2, A1);
}

std::string llvm::DOTGraphTraits<GraphVizOutputGraph>::getNodeLabel
    (const DataNode<Atom*, GraphVizEdge> *Node, const GraphVizOutputGraph &CG) {
  return Node->getData()->getStr();
}

std::string llvm::DOTGraphTraits<GraphVizOutputGraph>::getEdgeAttributes
    (const DataNode<Atom *, GraphVizEdge> *Node, ChildIteratorType T,
     const GraphVizOutputGraph &CG) {
  static const std::string EdgeTypeColors[2] = {"red", "blue" };
  static const std::string EdgeDirections[2] = { "forward", "both" };

  llvm::SmallVector<GraphVizEdge*, 2> Edges;
  Node->findEdgesTo(**T, Edges);
  assert(Edges.size() == 1 || Edges.size() == 2);

  // I've used a bit of a hack here because I can't find a clean way to
  // differentiate between multiple edges between the same pair of nodes.
  GraphVizEdge *GE = nullptr;
  auto EPair = std::make_pair(Node->getData(), (*T)->getData());
  for (auto *E : Edges) {
    if (E->Kind == GraphVizEdge::EK_Checked
        && CG.DoneChecked.find(EPair) == CG.DoneChecked.end()) {
      GE = E;
    } else if (E->Kind == GraphVizEdge::EK_Ptype
               && CG.DonePtyp.find(EPair) == CG.DonePtyp.end()) {
      GE = E;
    }
  }
  assert(GE != nullptr);
  if (GE->Kind == GraphVizEdge::EK_Checked)
    CG.DoneChecked.insert(EPair);
  else if (GE->Kind == GraphVizEdge::EK_Ptype)
    CG.DonePtyp.insert(EPair);

  return "color=" + EdgeTypeColors[GE->Kind] + ","
      + "dir=" + EdgeDirections[GE->IsBidirectional];
}

void GraphVizOutputGraph::mergeConstraintGraph(const ConstraintsGraph &Graph,
                                               GraphVizEdge::EdgeKind EK) {
  for (auto *N : Graph.Nodes) {
    auto *S = addVertex(N->getData());
    for (auto *E : N->getEdges()) {
      Atom *TargetData = E->getTargetNode().getData();
      auto *D = addVertex(TargetData);
      if (D->hasEdgeTo(*S)) {
        llvm::SmallVector<GraphVizEdge*, 2> Edges;
        D->findEdgesTo(*S, Edges);
        for (auto *OldE : Edges)
          if (OldE->Kind == EK)
            OldE->IsBidirectional = true;
      } else {
        GraphVizEdge *GE = new GraphVizEdge(*D, EK);
        connect(*S, *D, *GE);
      }
    }
  }
}

void GraphVizOutputGraph::dumpConstraintGraphs(const std::string &GraphDotFile,
                                               const ConstraintsGraph &Chk,
                                               const ConstraintsGraph &Pty) {
  GraphVizOutputGraph OutGraph;
  OutGraph.mergeConstraintGraph(Chk, GraphVizEdge::EK_Checked);
  OutGraph.mergeConstraintGraph(Pty, GraphVizEdge::EK_Ptype);

  std::error_code Err;
  llvm::raw_fd_ostream DotFile(GraphDotFile, Err);
  llvm::WriteGraph(DotFile, OutGraph);
  DotFile.close();
}