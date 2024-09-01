//=--AvarBoundsConflictResolver.h---------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the methods to fix bound conflicts in graphs
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_AVARBOUNDSCONFLICTRESOLVER_H
#define LLVM_CLANG_3C_AVARBOUNDSCONFLICTRESOLVER_H

#include "clang/3C/AVarBoundsInfo.h"
#include "clang/3C/AVarGraph.h"

class AVarBoundsConflictResolver {
public:
    // Fill WorkList with conflicting bounds. This list will be later used to
    // propagate the conflicts.
    void seedInitialWorkList(AVarBoundsInfo *BI,
                             AVarGraph &BKGraph,
                             std::set<BoundsKey> &WorkList);
    
    // Using the WorkList, propagate the conflicts to all conncted Nodes in the graph
    void propogateConflicts(const BoundsKey &N,
                            AVarBoundsInfo *BI,
                            AVarGraph &BKGraph,
                            std::set<BoundsKey> &WorkList);
    
    // Fix all conflicts in the graphs.
    void resolveConflicts(AVarBoundsInfo *BI);
};

#endif // LLVM_CLANG_3C_AVARBOUNDSCONFLICTRESOLVER_H
