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
typedef DataGraph<BoundsKey> AVarGraph;

#endif // _AVARGRAPH_H
