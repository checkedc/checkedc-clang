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

#include "ProgramVar.h"
#include <llvm/ADT/DirectedGraph.h>
#include <queue>

class AVarBoundsInfo;

class BoundsNode;
struct BoundsEdge;


class BoundsNode : public llvm::DGNode<BoundsNode, BoundsEdge> {
public:
  BoundsNode() = delete;
  BoundsNode(BoundsKey BK) : llvm::DGNode<BoundsNode, BoundsEdge>(), BK(BK) {}
  BoundsNode(BoundsKey BK, BoundsEdge &E) : llvm::DGNode<BoundsNode, BoundsEdge>(E), BK(BK) {}
  BoundsNode(const BoundsNode &N) : llvm::DGNode<BoundsNode, BoundsEdge>(N), BK(N.BK) {}
  BoundsNode(const BoundsNode &&N) : llvm::DGNode<BoundsNode, BoundsEdge>(std::move(N)), BK(N.BK) {}

  ~BoundsNode() { }

  BoundsNode &operator=(const BoundsNode &N) {
    DGNode::operator=(N);
    BK = N.BK;
    return *this;
  }

  BoundsNode &operator=(BoundsNode &&N) {
    DGNode::operator=(std::move(N));
    BK = N.BK;
    return *this;
  }

  BoundsKey getKey() const { return BK; }

  bool isEqualTo(const BoundsNode &B) const { return this->BK == B.BK; }

private:
  BoundsKey BK;
};

struct BoundsEdge : llvm::DGEdge<BoundsNode, BoundsEdge> {
  explicit BoundsEdge(BoundsNode &Node) : DGEdge(Node) {}
  explicit BoundsEdge(const BoundsEdge &E) : DGEdge(E) {}

  bool isEqualTo(const BoundsEdge &E) const {
    return this->getTargetNode() == E.getTargetNode();
  }
};

// Graph that keeps tracks of direct assignments between various variables.
class AVarGraph : public llvm::DirectedGraph<BoundsNode, BoundsEdge> {
public:
  ~AVarGraph();
  // Get all predecessors of the given bounds key K
  bool getPredecessors(BoundsKey K, std::set<BoundsKey> &Pred);
  bool getSuccessors(BoundsKey K, std::set<BoundsKey> &Succ);

  // Add edge between two bounds key. The flag BD indicates if the edge
  // is bidirectional.
  void addEdge(BoundsKey L, BoundsKey R, bool BD = true);

  void breadthFirstSearch(BoundsKey start, llvm::function_ref<void(BoundsKey)> Fn);


  void dumpCGDot(const std::string &GraphDotFile, AVarBoundsInfo *ABInfo);
  BoundsNode *addKey(BoundsKey K);
};

#endif // _AVARGRAPH_H
