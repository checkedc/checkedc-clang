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
  typedef adjacency_list<vecS, vecS, directedS, Atom*> DirectedGraphType;
  void addConstraint(Eq *C, Constraints &CS);
  void addConstraint(Geq *C, Constraints &CS);

  DirectedGraphType CG;
private:
};

#endif // _CONSTRAINTSGRAPH_H
