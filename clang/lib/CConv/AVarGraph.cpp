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

#include "clang/CConv/AVarBoundsInfo.h"
#include "clang/CConv/AVarGraph.h"
#include <iostream>

BoundsNode *AVarGraph::addKey(BoundsKey K) {
  BoundsNode *BN = new BoundsNode(K);
  auto *N = findNode(*BN);
  if (N != end()) {
    delete BN;
    return *N;
  }
  addNode(*BN);
  return BN;
}

void AVarGraph::addEdge(BoundsKey L, BoundsKey R, bool BD) {
  BoundsNode *BL = addKey(L);
  BoundsNode *BR = addKey(R);

  BoundsEdge *BRL = new BoundsEdge(*BL);
  connect(*BR, *BL, *BRL);
  if (BD) {
    BoundsEdge *BLR = new BoundsEdge(*BR);
    connect(*BL, *BR, *BLR);
  }
}

bool AVarGraph::getPredecessors(BoundsKey K, std::set<BoundsKey> &Pred) {
  auto *N = findNode(BoundsNode(K));
  if (N == end())
    return false;
  bool Any = false;
  for (auto *Neighbor : Nodes) {
    if (Neighbor == *N)
      continue;
    if (Neighbor->hasEdgeTo(**N)) {
      Any = true;
      BoundsKey Key = Neighbor->getKey();
      Pred.insert(Key);
    }
  }
  return Any;
}

bool AVarGraph::getSuccessors(BoundsKey K, std::set<BoundsKey> &Succ) {
  auto *N = findNode(BoundsNode(K));
  if (N == end())
    return false;
  auto Edges = (*N)->getEdges();
  if (Edges.empty())
    return false;
  for (auto *E : (*N)->getEdges()) {
    Succ.insert(E->getTargetNode().getKey());
  }
  return true;
}

void AVarGraph::breadthFirstSearch(BoundsKey start,
                                   llvm::function_ref<void(BoundsKey)> Fn) {
  std::queue<BoundsNode*> SearchQueue;
  std::set<BoundsNode*> VisitedSet;
  auto *N = findNode(BoundsNode(start));
  if (N == end())
    return;
  SearchQueue.push(*N);
  while (!SearchQueue.empty()) {
    BoundsNode *Node = SearchQueue.front();
    SearchQueue.pop();
    if (VisitedSet.find(Node) != VisitedSet.end())
      continue;
    VisitedSet.insert(Node);
    BoundsKey B = Node->getKey();
    Fn(B);
    for (auto *E : Node->getEdges())
      SearchQueue.push(&E->getTargetNode());
  }
}

AVarGraph::~AVarGraph() {
  for (BoundsNode *N : Nodes) {
    for (auto *E : N->getEdges())
      delete E;
    N->getEdges().clear();
    delete N;
    N = nullptr;
  }
  Nodes.clear();
}


//void AVarGraph::dumpCGDot(const std::string &GraphDotFile,
//                          AVarBoundsInfo *ABInfo) {
//  std::ofstream DotFile;
//  DotFile.open(GraphDotFile);
//  write_graphviz(DotFile, CG,
//                 [this, ABInfo] (std::ostream &out, unsigned v) {
//                   auto BK = CG[v];
//                   bool IsPtr = ABInfo->PointerBoundsKey.find(BK) !=
//                                ABInfo->PointerBoundsKey.end();
//                   bool IsArrPtr = ABInfo->ArrPointerBoundsKey.find(BK) !=
//                                   ABInfo->ArrPointerBoundsKey.end();
//                   // If this is a regular pointer? Ignore.
//                   if (IsPtr && !IsArrPtr) {
//                     return;
//                   }
//                   std::string ClrStr = IsPtr ? "red" : "blue";
//                   std::string LblStr =
//                       ABInfo->getProgramVar(CG[v])->verboseStr();
//                   std::string ShapeStr = "oval";
//                   if (IsArrPtr) {
//                     ClrStr = "green";
//                     ShapeStr = "note";
//                   } else {
//                     ShapeStr = "ellipse";
//                   }
//                   if (IsArrPtr) {
//                     // The pointer has bounds. Get the bounds.
//                     if (auto *B = ABInfo->getBounds(BK)) {
//                       ShapeStr = "tripleoctagon";
//                       LblStr += "(B:" + B->mkString(ABInfo) + ")";
//                       auto &ABStats = ABInfo->getBStats();
//                       if (ABStats.isDataflowMatch(BK)) {
//                         ShapeStr = "box";
//                       } else if(ABStats.isNamePrefixMatch(BK)) {
//                         ShapeStr = "pentagon";
//                       } else if(ABStats.isAllocatorMatch(BK)) {
//                         ShapeStr = "hexagon";
//                       } else if(ABStats.isVariableNameMatch(BK)) {
//                         ShapeStr = "septagon";
//                       } else if(ABStats.isNeighbourParamMatch(BK)) {
//                         ShapeStr = "octagon";
//                       }
//                     }
//                   }
//
//                   out << "[label=\"" << LblStr << "\", " <<
//                          "color=\"" << ClrStr << "\", " <<
//                          "shape=\"" << ShapeStr << "\"]";
//                 });
//  DotFile.close();
//}