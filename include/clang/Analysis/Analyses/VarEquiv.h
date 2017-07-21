//=------ VarEquiv.cpp - Analysis of equality of variables -----*- C++ --**-==/
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  Conservatively determine for each program point in a function which 
//  variables must be equal to each other, constants, or address-expressions 
//  whose values do not vary during during the lifetime of the function.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_VAREQUIV_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_VAREQUIV_H

#include "clang/AST/Decl.h"
#include "clang/AST/CanonBounds.h"

namespace clang {

class CFG;
class CFGBlock;
class SourceManager;

namespace PartitionRefinement {
  typedef int Element;
  class ElementMap;
  struct ListNode;
  class Set;
  class SetManager;

  // Represent a partitioning of a set of integers into
  // equivalence classes.  Provide operations for refining
  // the partition (dividing existing classes into smaller
  // classes).
  class Partition {
  public:
    Partition();
    ~Partition();
    /// \brief Add Elem to the equivalence class for Member.  Elem must
    /// currently be a member of a singleton equivalence class and
    /// cannot be a member of a non-trivial equivalence class.
    void add(Element Member, Element Elem);
    /// \brief Returns true if Elem is a member of a singleton
    /// equivalence class.
    bool isSingleton(Element Elem) const;
    /// \brief Refine the partition so that Elem is in a singleton
    /// equivalence class.
    void makeSingleton(Element Elem);
    /// \brief Return a representative element of the equivalence class
    /// containing Elem.
    Element getRepresentative(Element Elem) const;
    /// \brief Refine the existing partition by each of the equivalence
    /// classes in R.  For each equivalence class C in 'this' partition that
    /// contains an element of R, divide it into two equivalence classes:
    /// intersection(C, R) and C - R.
    void refine(const Partition *R);
    void dump(raw_ostream &OS, Element Elem) const;
    void dump(raw_ostream &OS) const;

  private:
    ListNode *add(Set *S, Element Elem);
    void remove_if_trivial(Set *S);
    void refine(const Set *S);
    void dump(raw_ostream &OS, Set *S) const;

    ElementMap *NodeMap;
    SetManager *Sets;
    std::vector<Set *> Scratch;
  };
}
}
#endif
