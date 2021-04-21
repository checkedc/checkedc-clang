//===------- AbstractSet.cpp: An abstract representation of memory -------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
//
//  This file implements methods to get or create a representation of sets of
//  identical lvalue expressions that refer to the same memory location.
//
//===---------------------------------------------------------------------===//

#include "clang/AST/AbstractSet.h"
#include "clang/AST/ExprUtils.h"

using namespace clang;

const AbstractSet *AbstractSetManager::GetOrCreateAbstractSet(Expr *E) {
  // Create a canonical form for E.
  PreorderAST *P = new PreorderAST(S.getASTContext(), E);
  P->Normalize();

  // Search for an existing PreorderAST that is equivalent to the canonical
  // form for E.
  auto I = SortedPreorderASTs.find(P);
  if (I != SortedPreorderASTs.end()) {
    PreorderAST *ExistingCanonicalForm = *I;
    // If an AbstractSet exists in PreorderASTAbstractSetMap whose CanonicalForm
    // is equivalent to ExistingCanonicalForm, then that AbstractSet is the
    // one that contains E.
    auto It = PreorderASTAbstractSetMap.find(ExistingCanonicalForm);
    if (It != PreorderASTAbstractSetMap.end()) {
      P->Cleanup();
      delete P;
      return It->second;
    }
  }

  // If there is no existing AbstractSet that contains E, create a new
  // AbstractSet that contains E.
  const AbstractSet *A = new AbstractSet(*P, E);
  SortedPreorderASTs.emplace(P);
  PreorderASTAbstractSetMap[P] = A;
  return A;
}

const AbstractSet *AbstractSetManager::GetOrCreateAbstractSet(const VarDecl *V) {
  // Compute the DeclRefExpr that is a use of V. This DeclRefExpr is needed
  // in order to get or create the AbstractSet that contains V.
  // The VarUses map may not contain a DeclRefExpr for V even if V has declared
  // bounds. However, we still need to create an AbstractSet for V so that its
  // bounds can be checked. For example, consider:
  // void f(_Array_ptr<int> p : bounds(q, q + i), _Array_ptr<int> q, int i) {
  //   i = 0;
  // }
  // The parameter declaration `p` does not have a DeclRefExpr in the
  // VarUses map, but the statement `i = 0` invalidates the observed bounds
  // of p.
  DeclRefExpr *VarUse = nullptr;
  auto It = VarUses.find(V);
  if (It != VarUses.end()) {
    VarUse = It->second;
  } else {
    VarDecl *D = const_cast<VarDecl *>(V);
    VarUse = ExprCreatorUtil::CreateVarUse(S, D);
    VarUses[V] = VarUse;
  }
  return GetOrCreateAbstractSet(VarUse);
}
