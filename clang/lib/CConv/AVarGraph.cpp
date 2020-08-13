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
#include <boost/graph/graphviz.hpp>
#include <iostream>

void AVarGraph::addEdge(BoundsKey L, BoundsKey R, bool BD) {
  auto V1 = addVertex(L);
  auto V2 = addVertex(R);
  add_edge(V2, V1, CG);
  if (BD) {
    add_edge(V1, V2, CG);
  }
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

void AVarGraph::dumpCGDot(const std::string &GraphDotFile,
                          AVarBoundsInfo *ABInfo) {
  std::ofstream DotFile;
  DotFile.open(GraphDotFile);
  write_graphviz(DotFile, CG,
                 [this, ABInfo] (std::ostream &out, unsigned v) {
                   auto BK = CG[v];
                   bool IsPtr = ABInfo->PointerBoundsKey.find(BK) !=
                                ABInfo->PointerBoundsKey.end();
                   bool IsArrPtr = ABInfo->ArrPointerBoundsKey.find(BK) !=
                                   ABInfo->ArrPointerBoundsKey.end();
                   // If this is a regular pointer? Ignore.
                   if (IsPtr && !IsArrPtr) {
                     return;
                   }
                   std::string ClrStr = IsPtr ? "red" : "blue";
                   ProgramVar *Tmp = ABInfo->getProgramVar(CG[v]);
                   std::string LblStr = "Temp";
                   if (Tmp != nullptr)
                       LblStr = Tmp->verboseStr();
                   std::string ShapeStr = "oval";
                   if (IsArrPtr) {
                     ClrStr = "green";
                     ShapeStr = "note";
                   } else {
                     ShapeStr = "ellipse";
                   }
                   if (IsArrPtr) {
                     // The pointer has bounds. Get the bounds.
                     if (auto *B = ABInfo->getBounds(BK)) {
                       ShapeStr = "tripleoctagon";
                       LblStr += "(B:" + B->mkString(ABInfo) + ")";
                       auto &ABStats = ABInfo->getBStats();
                       if (ABStats.isDataflowMatch(BK)) {
                         ShapeStr = "box";
                       } else if(ABStats.isNamePrefixMatch(BK)) {
                         ShapeStr = "pentagon";
                       } else if(ABStats.isAllocatorMatch(BK)) {
                         ShapeStr = "hexagon";
                       } else if(ABStats.isVariableNameMatch(BK)) {
                         ShapeStr = "septagon";
                       } else if(ABStats.isNeighbourParamMatch(BK)) {
                         ShapeStr = "octagon";
                       }
                     }
                   }

                   out << "[label=\"" << LblStr << "\", " <<
                          "color=\"" << ClrStr << "\", " <<
                          "shape=\"" << ShapeStr << "\"]";
                 });
  DotFile.close();
}