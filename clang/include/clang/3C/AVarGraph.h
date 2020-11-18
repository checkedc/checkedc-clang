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

#include "ConstraintsGraph.h"

// Graph that keeps tracks of direct assignments between various variables.
class AVarGraph : public DataGraph<BoundsKey> {
public:
  AVarGraph(AVarBoundsInfo *ABInfo) : DataGraph(), ABInfo(ABInfo) {}

private:
  friend struct llvm::DOTGraphTraits<AVarGraph>;
  AVarBoundsInfo *ABInfo;
};

namespace llvm {
template <> struct GraphTraits<AVarGraph> {
  using NodeRef = DataNode<BoundsKey> *;
  using EdgeType = DataEdge<BoundsKey> *;
  using nodes_iterator = AVarGraph::iterator;

  static NodeRef GetTargetNode(EdgeType P) { return &P->getTargetNode(); }

  using ChildIteratorType =
      mapped_iterator<typename DataNode<BoundsKey>::iterator,
                      decltype(&GetTargetNode)>;

  static nodes_iterator nodes_begin(const AVarGraph &G) {
    return const_cast<AVarGraph &>(G).Nodes.begin();
  }

  static nodes_iterator nodes_end(const AVarGraph &G) {
    return const_cast<AVarGraph &>(G).Nodes.end();
  }

  static ChildIteratorType child_begin(NodeRef N) {
    return ChildIteratorType(N->begin(), &GetTargetNode);
  }

  static ChildIteratorType child_end(NodeRef N) {
    return ChildIteratorType(N->end(), &GetTargetNode);
  }
};

template <>
struct DOTGraphTraits<AVarGraph> : public llvm::DefaultDOTGraphTraits,
                                   llvm::GraphTraits<GraphVizOutputGraph> {
  DOTGraphTraits(bool simple = false) : DefaultDOTGraphTraits(simple) {}

  std::string getNodeAttributes(const DataNode<BoundsKey> *Node,
                                const AVarGraph &CG);
  std::string getNodeLabel(const DataNode<BoundsKey> *Node, const AVarGraph &G);
};
} // namespace llvm
#endif // _AVARGRAPH_H