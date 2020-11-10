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

#include "clang/3C/AVarBoundsInfo.h"
#include <iostream>

std::string llvm::DOTGraphTraits<AVarGraph>::getNodeAttributes(
    const DataNode<BoundsKey> *Node, const AVarGraph &G) {
  AVarBoundsInfo *ABInfo = G.ABInfo;
  BoundsKey BK = Node->getData();
  bool IsPtr =
      ABInfo->PointerBoundsKey.find(BK) != ABInfo->PointerBoundsKey.end();
  bool IsArrPtr =
      ABInfo->ArrPointerBoundsKey.find(BK) != ABInfo->ArrPointerBoundsKey.end();
  // If this is a regular pointer? Ignore.
  if (IsPtr && !IsArrPtr)
    return "";

  std::string ClrStr = IsPtr ? "red" : "blue";
  std::string ShapeStr = "oval";
  if (IsArrPtr) {
    ClrStr = "green";
    ShapeStr = "note";
  } else {
    ShapeStr = "ellipse";
  }
  if (IsArrPtr) {
    // The pointer has bounds. Get the bounds.
    if (ABInfo->getBounds(BK)) {
      ShapeStr = "tripleoctagon";
      auto &ABStats = ABInfo->getBStats();
      if (ABStats.isDataflowMatch(BK)) {
        ShapeStr = "box";
      } else if (ABStats.isNamePrefixMatch(BK)) {
        ShapeStr = "pentagon";
      } else if (ABStats.isAllocatorMatch(BK)) {
        ShapeStr = "hexagon";
      } else if (ABStats.isVariableNameMatch(BK)) {
        ShapeStr = "septagon";
      } else if (ABStats.isNeighbourParamMatch(BK)) {
        ShapeStr = "octagon";
      }
    }
  }
  return "color=\"" + ClrStr + "\", " + "shape=\"" + ShapeStr + "\"";
}

std::string
llvm::DOTGraphTraits<AVarGraph>::getNodeLabel(const DataNode<BoundsKey> *Node,
                                              const AVarGraph &G) {
  AVarBoundsInfo *ABInfo = G.ABInfo;
  BoundsKey BK = Node->getData();
  ProgramVar *Tmp = ABInfo->getProgramVar(BK);
  std::string LblStr = "Temp";
  if (Tmp != nullptr)
    LblStr = Tmp->verboseStr();
  bool IsArrPtr =
      ABInfo->ArrPointerBoundsKey.find(BK) != ABInfo->ArrPointerBoundsKey.end();
  if (IsArrPtr)
    if (auto *B = ABInfo->getBounds(BK))
      LblStr += "(B:" + B->mkString(ABInfo) + ")";
  return LblStr;
}