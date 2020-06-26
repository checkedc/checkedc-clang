//=--AVarGraph.cpp------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of methods in AVarGraph.h
//
//===----------------------------------------------------------------------===//

#include "clang/CConv/AVarGraph.h"

void AVarGraph::addEdge(BoundsKey L, BoundsKey R) {
  auto V1 = addVertex(L);
  auto V2 = addVertex(R);
  add_edge(V2, V1, CG);
}

bool AVarGraph::getPredecessors(BoundsKey K, std::set<BoundsKey> &Pred) {
  bool RetVal = false;
  typename graph_traits<DirectedGraphType>::in_edge_iterator I, IEnd;
  if (BkeyToVDMap.find(K) != BkeyToVDMap.end()) {
    vertex_t VerIdx = BkeyToVDMap[K];
    for (boost::tie(I, IEnd) = in_edges(VerIdx, CG); I != IEnd; ++I) {
      auto PredIdx = boost::source ( *I, CG );
      Pred.insert(CG[PredIdx]);
      RetVal = true;
    }
  }
  return RetVal;
}

bool AVarGraph::getSuccessors(BoundsKey K, std::set<BoundsKey> &Succ) {
  bool RetVal = false;
  typename graph_traits<DirectedGraphType>::out_edge_iterator I, IEnd;
  if (BkeyToVDMap.find(K) != BkeyToVDMap.end()) {
    vertex_t VerIdx = BkeyToVDMap[K];
    for (boost::tie(I, IEnd) = out_edges(VerIdx, CG); I != IEnd; ++I) {
      auto SucIdx = boost::target ( *I, CG );
      Succ.insert(CG[SucIdx]);
      RetVal = true;
    }
  }
  return RetVal;
}