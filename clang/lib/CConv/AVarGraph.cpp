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