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