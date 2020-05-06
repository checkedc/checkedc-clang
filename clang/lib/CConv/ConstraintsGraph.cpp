//=--ConstraintsGraph.cpp-----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of methods in ConstraintsGraph.cpp
//
//===----------------------------------------------------------------------===//

#include "clang/CConv/ConstraintsGraph.h"

void ConstraintsGraph::addConstraint(Eq *C, Constraints &CS) {
  // This is to make sure we always use same VarAtom* for a
  // vertex.
  VarAtom *VA1 = CS.getOrCreateVar(C->getLHS()->getLoc());
  VarAtom *VA2 = CS.getOrCreateVar(C->getRHS()->getLoc());
  auto V1 = add_vertex(VA1, CG);
  auto V2 = add_vertex(VA2, CG);
  // Add edges in both the directions.
  add_edge(V1, V2, CG);
  add_edge(V2, V2, CG);
}

void ConstraintsGraph::addConstraint(Geq *C, Constraints &CS) {
  Atom *A1 = C->getLHS();
  if (VarAtom *VA1 = clang::dyn_cast<VarAtom>(A1)) {
    A1 = CS.getOrCreateVar(VA1->getLoc());
  }
  Atom *A2 = C->getRHS();
  if (VarAtom *VA2 = clang::dyn_cast<VarAtom>(A2)) {
    A2 = CS.getOrCreateVar(VA2->getLoc());
  }
  // Here, LHS >= RHS
  // So, edge from RHS -> LHS
  auto V1 = add_vertex(A1, CG);
  auto V2 = add_vertex(A2, CG);
  add_edge(V2, V1, CG);
}