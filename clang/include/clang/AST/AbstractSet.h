//===------- AbstractSet.h: An abstract representation of memory -------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-------------------------------------------------------------------===//
//
//  This file defines the interface for a representation of sets of
//  identical lvalue expressions that refer to the same memory location.
//
//===-------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ABSTRACT_SET_H
#define LLVM_CLANG_ABSTRACT_SET_H

#include <set>
#include "clang/AST/ASTContext.h"
#include "clang/AST/CanonBounds.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PreorderAST.h"
#include "clang/Sema/CheckedCAnalysesPrepass.h"
#include "clang/Sema/Sema.h"

namespace clang {
  using Result = Lexicographic::Result;

  // The AbstractSet class represents an abstraction of memory. If two lvalue
  // expressions e1 and e2 belong to the same AbstractSet, then e1 and e2
  // point to the same contiguous block of memory locations (i.e. e1 and e2
  // point to the same location and range in memory).
  class AbstractSet {
  private:
    // Canonical form of all lvalue expressions that belong to this AbstractSet.
    // Two lvalue expressions e1 and e2 belong to the same AbstractSet if and
    // only if e1 and e2 have the same canonical form.
    PreorderAST CanonicalForm;

    // LValue expression that is a representative of all lvalue expressions that
    // belong to this AbstractSet. This can be used in bounds validation to:
    // 1. Get the Decl for the representative. This is used to determine the
    //    location of the note that specifies the declared (target) bounds.
    // 2. Get the target bounds for the representative. This is used to prove
    //    or disprove that the inferred bounds for the expressions in this
    //    AbstractSet imply the target bounds. All lvalue expressions in this
    //    AbstractSet have the same target bounds as the representative.
    //    Bounds validation must use existing bounds checking methods in the
    //    CheckBoundsDeclarations class to compute the target bounds for the
    //    representative expression.
    Expr *Representative;

  public:
    AbstractSet(PreorderAST P, Expr *Rep) :
      CanonicalForm(P), Representative(Rep) {}

    Expr *GetRepresentative() const {
      return Representative;
    }

    // Returns the NamedDecl, if any, associated with the Representative
    // expression for this AbstractSet.
    // This NamedDecl is used by bounds declaration checking to emit
    // diagnostics for statements that invalidate the inferred bounds of
    // the lvalue expressions in the AbstractSet.
    const NamedDecl *GetDecl() const {
      if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Representative))
        return DRE->getDecl();
      if (MemberExpr *ME = dyn_cast<MemberExpr>(Representative))
        return ME->getMemberDecl();
      return nullptr;
    }

    void PrettyPrint(llvm::raw_ostream &OS, ASTContext &Ctx) const {
      CanonicalForm.PrettyPrint(OS, Ctx);
    }

    // The comparison between two AbstractSets is the same as the
    // lexicographic comparison between their CanonicalForms.
    Result Compare(AbstractSet &Other) const {
      return CanonicalForm.Compare(Other.CanonicalForm);
    }

    bool operator<(AbstractSet &Other) const {
      return Compare(Other) == Result::LessThan;
    }
    bool operator==(AbstractSet &Other) const {
      return Compare(Other) == Result::Equal;
    }
  };

  class AbstractSetManager {
  private:
    Sema &S;

    // VarUses maps a VarDecl to the DeclRefExpr (if any) that is the first
    // use of the VarDecl. If a VarDecl V has an entry in VarUses, the
    // DeclRefExpr for V is used to get or create the AbstractSet for V.
    // Otherwise, a use of V is constructed and added to VarUses. This created
    // use of V is currently not released. It should be rare that a use of V
    // needs to be created, since this should only occur if:
    // V does not occur in any declared bounds expressions, and:
    //  1. V is unused within the body of the function, or:
    //  2. V does not have a declared bounds expression.
    // In order to get or create the AbstractSet for V, any use of V would
    // be sufficient. We choose the first use of V.
    VarUsageTy &VarUses;

    // Maintain a sorted set of PreorderASTs that have been created while
    // traversing a function. A binary search in this set is used to determine
    // whether an lvalue expression belongs to an existing AbstractSet (an
    // AbstractSet whose CanonicalForm is in the SortedPreorderASTs set).
    // An std::set is used for this set since std::sets are sorted by default.
    // Here, the PreorderASTComparer is used to sort the PreorderASTs
    // lexicographically. This avoids the need for a linear search through
    // SortedPreorderASTs in GetOrCreateAbstractSet.
    std::set<PreorderAST *, PreorderASTComparer> SortedPreorderASTs;

    // Map each PreorderAST P that has been created while traversing a function
    // to the AbstractSet whose CanonicalForm is P. This is used to retrieve
    // the AbstractSet whose CanonicalForm already exists in SortedPreorderASTs
    // (if any).
    llvm::DenseMap<PreorderAST *, const AbstractSet *> PreorderASTAbstractSetMap;

  public:
    AbstractSetManager(Sema &S, VarUsageTy &VarUses) :
      S(S), VarUses(VarUses) {}

    ~AbstractSetManager() {
      Clear();
    }

    // Returns the AbstractSet that contains the lvalue expression E. If
    // there is an AbstractSet A in SortedAbstractSets that contains E,
    // GetOrCreateAbstractSet returns A. Otherwise, it creates a new
    // AbstractSet for E.
    const AbstractSet *GetOrCreateAbstractSet(Expr *E);

    // Returns the AbstractSet that contains a use of the VarDecl.
    const AbstractSet *GetOrCreateAbstractSet(const VarDecl *V);

    // Clears the storage of the PreorderASTs and AbstractSets.
    void Clear() {
      for (const auto &Pair : PreorderASTAbstractSetMap) {
        Pair.first->Cleanup();
        delete Pair.first;
        delete Pair.second;
      }
      SortedPreorderASTs.clear();
      PreorderASTAbstractSetMap.clear();
    }
  };
} // end namespace clang

#endif
