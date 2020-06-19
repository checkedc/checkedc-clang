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

  // If we haven't seen the Atom? Insert into the graph.
  if (AtomToVDMap.find(A) == AtomToVDMap.end()) {
    auto Vidx = add_vertex(A, CG);
    AtomToVDMap[A] = Vidx;
  }
  return AtomToVDMap[A];
}

std::set<ConstAtom*> &ConstraintsGraph::getAllConstAtoms() {
  return AllConstAtoms;
}

void ConstraintsGraph::addConstraint(Geq *C, Constraints &CS) {
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

void ConstraintsGraph::dumpCGDot(const std::string& GraphDotFile) {
   std::ofstream DotFile;
   DotFile.open(GraphDotFile);
   write_graphviz(DotFile, CG, [&] (std::ostream &out, unsigned v) {
     out << "[label=\"" << CG[v]->getStr() << "\"]";
   });
   DotFile.close();
}