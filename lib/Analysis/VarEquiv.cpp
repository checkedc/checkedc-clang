//=------ VarEquiv.cpp - Analysis of equality of variables -----*- C++ --**-==//
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

#include "clang/Analysis/Analyses/VarEquiv.h"
#include "clang/Analysis/CFG.h"
#include "clang/AST/StmtVisitor.h"

using namespace clang;

namespace {

// Represent a set of equivalence classes of the set of integers in 0 ... n - 1,
// providing efficient operations for partition refinement of the equivalence 
// classes.
//
// Partition refinements works as follows: given a partitioning of a set of
// set of integers into sets S1, ... SN, and set R, divide each set SI 
// that contains a mmeber of R into two new sets:
// - SI intersected with R
// - SI minus R
//
// This can be used to intersect multiple sets of equivalence 
// classes efficiently.
//
// With  the right representation, these operations can be done in
// time proportional to the number of elements in R.
//
// We optimize this for space usage:
// 1. We optionally omit trivial equivalence classes with singleton
//    members.
/// 2. We use space proportional to the number of elements in the
//    in non-trivial equivalence classes.
// 
class PartitionRefinement {
public:
  typedef int Element;

private:
  // We represent each equivalence class as an unordered doubly-linked list so 
  // that we can easily add/remove elements from the set.
  class Set;

  struct ListNode {
    ListNode(Element E, Set *S) : Elem(E), Set(S), Prev(nullptr), Next(nullptr) {
    }

    Element Elem;
    Set *Set;
    ListNode *Prev;
    ListNode *Next;
  };

  class Set {
  public:
    struct ListNode *Head;
    Set *Intersected; // scratch pointer used during refinement.
    int Id;                  // Internal id of set; may change as sets are removed/added.

    Set() : Head(nullptr), Intersected(nullptr), Id(Sentinel) {}

    bool isEmpty() {
      return Head == nullptr;
    }

    bool isSingleton() {
      return Head != nullptr && Head->Prev == nullptr && Head->Next == nullptr;
    }

    static const int Sentinel = -1;
  };

  // SetManager tracks a list of sets.
  class SetManager {
  private:
    std::vector<Set *> Sets;

  public:
    // Add S to the list of sets. Return the position where S was
    // added.  We don't set the id because sometimes a set needs
    // to be tracked in two lists.
    int add(Set *S) {
      int Id = Sets.size();
      Sets.push_back(S);
      return Id;
    }

    void assign(SetManager *SM) {
      Sets.assign(SM->Sets.begin(), SM->Sets.end());
    }

    Set *get(unsigned i) {
      if (i >= 0 && i < Sets.size())
        return Sets[i];
      else
        return nullptr;
    }

    // Remove a set from the list of set by swapping the 
    // set at the end of the list with this set.  Updates
    // the Id for the swapped set.
    void remove(Set *S) {
      int Id = S->Id;
      int size = Sets.size();
      assert(Id >= 0 && Id < size);
      if (Id != size - 1) {
        Set *SwapTarget = Sets[size - 1];
        Sets[Id] = SwapTarget;
        SwapTarget->Id = Id;
      }
      Sets.pop_back();
    }

    void clear() {
      Sets.clear();
    }

    int size() {
      return Sets.size();
    }
  };

  // Map an equivalence set element to its list node.
  class ElementMap {
  private:
     std::map<Element, ListNode *> Tree;

  public:
    ElementMap(int Size) {
    }
      
    ListNode *get(Element Elem) {
      auto Lookup = Tree.find(Elem);
      if (Lookup == Tree.end())
        return nullptr;
      else
        return Lookup->second;
    }

    void remove(Element Elem) {
      auto Lookup = Tree.find(Elem);
      if (Lookup == Tree.end())
        return;
      Tree.erase(Lookup);
    }

    void set(Element Elem,ListNode *Node) {
      Tree[Elem] = Node;
    }
  };

  ElementMap NodeMap;
  SetManager Sets;
  SetManager Scratch;

  // Unlink the node from its current set.
  void unlinkNode(ListNode *Node) {
    if (Node->Prev) {
      Node->Prev->Next = Node->Next;
    }
    if (Node->Next) {
      Node->Next->Prev = Node->Prev;
    }
    if (Node == Node->Set->Head) {
      assert(Node->Prev == nullptr);
      Node->Set->Head = Node->Next;
    }
  }

  // Link the node to set S
  void linkNode(Set *S, ListNode *Node) {
    Node->Set = S;
    ListNode *Head = S->Head;
    Node->Prev = nullptr;
    Node->Next = Head;
    S->Head = Node;
    if (Head != nullptr) {
      Head->Prev = Node;
    }
  }

  // Move a node from is current set to set S.
  void moveNode(Set *S, ListNode *Node) {
    unlinkNode(Node);
    linkNode(S, Node);
  }

  void remove_if_trivial(Set *S) {
    if (S->isEmpty()) {
      Sets.remove(S);
      delete S;
    }
    else if (S->isSingleton()) {
      Sets.remove(S);
      NodeMap.remove(S->Head->Elem);
      delete S->Head;
      delete S;
    }
  }

public:
  PartitionRefinement(int Size) : NodeMap(Size) {
  };

  // Add Elem to the set S.  It is an error if Elem is already a member 
  // of another set.
  ListNode *add(Set *S, Element Elem) {
    assert(NodeMap.get(Elem) == nullptr || NodeMap.get(Elem)->Set != S);
    ListNode *Node = new ListNode(Elem, S);
    NodeMap.set(Elem, Node);
    linkNode(S, Node);
    return Node;
  }

  // Add Elem to the set S for Member.  If Member does not have
  // a set, create a new set to contain Elem and Member.
  // It is an error of Elem is already a member of another set.
  void add(Element Member, Element Elem) {
    ListNode *Node = NodeMap.get(Member);
    if (Node == nullptr) {
      Set *S = new Set();
      int Id = Sets.add(S);
      S->Id = Id;
      Node = add(S , Member);
    }
    add(Node->Set, Elem);
  }

  bool isMember(Element Elem) {
    return NodeMap.get(Elem) != nullptr;
  }

  // Remove Elem from its current equivalence class.  Elem
  // becomes a member of a singleton equivalence class.
  void remove(Element Elem) {
    ListNode *Node = NodeMap.get(Elem);
    if (Node != nullptr) {
      Set *S = Node->Set;
      assert(S != nullptr);
      unlinkNode(Node);
      NodeMap.remove(Elem);
      delete Node;
      remove_if_trivial(S); // S may point to freed memory after this.
    }
  }

private:
  // For each equivalence class C with a member in S,
  // split C into two sets:
  // * C intersected with S
  // * C - S.
  //
  // S must be a set from a different PartitionEquivalence.
  // We create a new set for C intersectd with S and use
  // the original set for S to hold C - S.

  void refine(Set *S) {  // TODO: mark as const
    Scratch.clear();
    ListNode *Current = S->Head;
    while (Current != nullptr) {
      Element CurrentElem = Current->Elem;
      ListNode *Target = NodeMap.get(CurrentElem);
      if (Target != nullptr) {
        Set *TargetSet = Target->Set;
        Set *Intersected = TargetSet->Intersected;
        if (Intersected == nullptr) {
          Intersected = new Set();
          int Id = Sets.add(Intersected);
          Intersected->Id = Id;
          TargetSet->Intersected = Intersected;
          Scratch.add(TargetSet);
        }
        moveNode(Intersected, Target);
      }
    }
    unsigned count = Scratch.size();
    for (unsigned i = 0; i < count; i++) {
      Set *Split = Scratch.get(i);
      Split->Intersected = nullptr;
      Set *Intersected = Split->Intersected;
      remove_if_trivial(Split);
      remove_if_trivial(Intersected);
    }
    Scratch.clear();
  }

 public:
  void refine(PartitionRefinement *R) { // TODO: mark R as const?
    assert(R != this);
    // First check that all elements of the current equivalence classes
    // are members of at least one equivalence class in R.  Elements
    // not in any equivalence class in R are in singleton equivalence
    // classes.  These are not represented in sets and need to be deleted.

    Scratch.assign(&Sets);  // avoid modifying list of sets we are iterating over.
    unsigned count = Scratch.size();
    for (unsigned i = 0; i < count; i++) {
      Set *S = Scratch.get(i);
      ListNode *Current = S->Head;
      while (Current != nullptr) {
        // Save next pointer now in case we delete Current.
        ListNode *Next = Current->Next;
        if (!R->isMember(Current->Elem)) {
          unlinkNode(Current);
          NodeMap.remove(Current->Elem);
          delete Current;
        }
        Current = Next;
      }
      remove_if_trivial(S); 
    }

    count = R->Sets.size();
    for (unsigned i = 0; i < count; i++)
      refine(R->Sets.get(i));
  }

  // If Elem is a member of a set, return the representative
  // element in Representative.  The return value indicates 
  // if Elem was a member of a set and Representative is valid.
  bool getRepresentative(Element Elem,Element &Representative) {
    ListNode *Node = NodeMap.get(Elem);
    if (Node == nullptr) {
      Representative = 0;
      return false;
    } else {
      Representative = Node->Set->Head->Elem;
      return true;
    }
  }

private:
  void Dump(raw_ostream &OS, Set *S) {
    ListNode *Current = S->Head;
    OS << "Set ";
    OS << S->Id;
    OS << "{";
    OS << Current->Elem;
    while (Current != nullptr) {
      Current = Current->Next;
      OS << ", ";
      OS << Current->Elem;
    }
    OS << "}";
  }

public:
  // Dump the set that Elem is equivalent to.
  void Dump(raw_ostream &OS, Element Elem) {
    ListNode *Node = NodeMap.get(Elem);
    OS << Elem;
    OS << ": ";
    if (Node == nullptr) {
      OS << "Nothing";
      return;
    }
    Dump(OS, Node->Set);
    OS << "\n";
  }

  // Dump all the sets
  void Dump(raw_ostream &OS) {
    unsigned Count = Sets.size();
    if (Count == 0)
      OS << "Equivalence classes are empty\n";
    else
      OS << "Equivalence classes:\n";
      
    for (unsigned i = 0; i < Count; i++) {
      Dump(OS, Sets.get(i));
      OS << "\n";
    }
  }
};
} // namespace

namespace {
class AddressTakenAnalysis: public StmtVisitor<AddressTakenAnalysis> {
private:
  SmallVector<VarDecl *, 8> Vars;

  AddressTakenAnalysis() {}

public:
  void VisitUnaryAddrOf(const UnaryOperator *E) {
    Expr *Operand = E->getSubExpr();
    DeclRefExpr *DR = dyn_cast<DeclRefExpr>(Operand);
    if (DR) {
      VarDecl *VD = dyn_cast<VarDecl>(DR->getDecl());
      if (VD)
        Vars.push_back(VD);
      return;
    }
  }

  void analyze(Stmt *Body) {
    Visit(Body);
    std::sort(Vars.begin(), Vars.end());
    std::unique(Vars.begin(), Vars.end());
  }

  bool isAddressTaken(VarDecl *VD) {
    return std::binary_search(Vars.begin(), Vars.end(), VD);
  }
};
}

namespace {
// Determine whether an expression is invariant during the execution of a function.
// By invariant, we mean that the expression produces the same value or lvalue every time
// it is evaluated during the execution of the function body.
class InvariantExpr : StmtVisitor<InvariantExpr> {
public:
  bool isInvariant(Expr *E) {
    switch (E->getStmtClass()) {
      case Stmt::NoStmtClass:
  #define ABSTRACT_STMT(Kind)
  #define STMT(Kind, Base) case Expr::Kind##Class:
  #define EXPR(Kind, Base)
  #include "clang/AST/StmtNodes.inc"
        llvm_unreachable("should not check invariance of a statement");
      case Expr::IntegerLiteralClass:
      case Expr::FloatingLiteralClass:
      case Expr::ImaginaryLiteralClass:
      case Expr::StringLiteralClass:
      case Expr::CharacterLiteralClass:
      case Expr::UnaryExprOrTypeTraitExprClass:
      case Expr::OffsetOfExprClass:
      // DeclRefs evalute to either constants or lvalues, so they are invariant
      // during the lifetime of a function.
      case Expr::DeclRefExprClass:
      // Compound literals evaluate to an lvalue that should always be the
      // same during the execution of a function body.  The object pointed to
      // by the lvalue may differ, but that doesn't matter for invariance.
      case Expr::CompoundLiteralExprClass:         
        return true;

      // The following expressions are not considered invariant because their
      // values may change.

      case Expr::PredefinedExprClass:
      case Expr::CallExprClass:
      case Expr::CompoundAssignOperatorClass:
      case Expr::ExtVectorElementExprClass:
      // GNU extension
      case Expr::StmtExprClass:
      // Clang extensions
      case Expr::ShuffleVectorExprClass:
      case Expr::BlockExprClass:
      case Expr::OpaqueValueExprClass:
      case Expr::TypoExprClass:
        return false; 
      // Member expressions produce lvalues.  There are two
      // forms: "." and "->".  e1->f is shorthand for
      // (*e1).f.  Both forms are invariant if the member
       // base expression is invariant.
      case Expr::MemberExprClass:
        break;
      // Array subscriptiong also produces an lvalue.  It is 
      // invariant if both of its expressions are invariant.
      case Expr::ArraySubscriptExprClass:
        break;
      case Expr::UnaryOperatorClass: {
        UnaryOperator *UO = dyn_cast<UnaryOperator>(E);
        if (!UO) {
          llvm_unreachable("unexpected cast failure");
          return false;
        }
        UnaryOperatorKind Opcode = UO->getOpcode();
        // Unary arithmetic operators are invariant if their
        // operands are invariant.
        if (UO->isArithmeticOp(Opcode))
          break;
        // Deref takes a pointer-typed value and converts it to an lvalue.
        // It is invariant if its operand is invariant.
        if (Opcode == UnaryOperatorKind::UO_Deref)
          break;

        // Address-of takes an lvalue and converts it to a pointer.  It
        // is invariant of the lvalue-producing expression is invariant.           
        if (Opcode == UnaryOperatorKind::UO_AddrOf)
          break;

        // Other unary operators are increment/decrement forms, which are 
        // not invariant because they dereference a lvalue.
        return false;
      }
      case Expr::BinaryOperatorClass:
        break;

      case Expr::ImplicitCastExprClass: {
        // The clang IR makes the conversion of lvalues to rvalues explicit.
        // This conversion reads the memory pointed to by the lvalue, so it
        // is not invariant.
        ImplicitCastExpr *IC = dyn_cast<ImplicitCastExpr>(E);
        if (IC->getCastKind() == CK_LValueToRValue)
          return false;
        break;  
      }

      // The following expressions are invariant if all their operands are invariant.
      case Expr::ParenExprClass:
      case Expr::BinaryConditionalOperatorClass:      
      case Expr::CStyleCastExprClass:
      case Expr::VAArgExprClass:
      case Expr::GenericSelectionExprClass:
      // Atomic expressions.
      case Expr::AtomicExprClass:
      // GNU Extensions.
      case Expr::AddrLabelExprClass:
      case Expr::ChooseExprClass:
      case Expr::GNUNullExprClass:
      // Clang extension
      case Expr::ConvertVectorExprClass:         
        break;

      // Bounds expressions
      case Expr::NullaryBoundsExprClass:
      case Expr::CountBoundsExprClass:
      case Expr::RangeBoundsExprClass:
      case Expr::InteropTypeBoundsAnnotationClass:
      case Expr::PositionalParameterExprClass:
      case Expr::BoundsCastExprClass:
        llvm_unreachable("should not be analyzing bounds expressions");
        return false;
      default:
        llvm_unreachable("unexpected expression kind");
    }

    // Check that child expressions are invariant.
    for (auto I = E->child_begin(); I != E->child_end(); ++I) {
      Stmt *Child = *I;
      if (!Child)
        return false;
      Expr *ExprChild = dyn_cast<Expr>(Child);
      if (!ExprChild) {
        llvm_unreachable("dyn_cast failed");
        return false;
      }
      if (!isInvariant(ExprChild))
        return false;
    }

    return true;
  }
};
}

namespace {
class FindInterestingVars : public StmtVisitor<FindInterestingVars> {
private:
  SmallVector<VarDecl *, 8> Vars;
  AddressTakenAnalysis &AddressTaken;

public:

  FindInterestingVars(AddressTakenAnalysis &ATA) :AddressTaken(ATA) {
  }

  void VisitBinaryOperator(const BinaryOperator *BO) {
  }

  void VisitUnaryAddrOf(const UnaryOperator *E) {
    Expr *Operand = E->getSubExpr();
    DeclRefExpr *DR = dyn_cast<DeclRefExpr>(Operand);
    if (DR) {
      VarDecl *VD = dyn_cast<VarDecl>(DR->getDecl());
      if (VD)
        Vars.push_back(VD);
      return;
    }
  }

  void analyze(Stmt *Body) {
    Visit(Body);
    std::sort(Vars.begin(), Vars.end());
    std::unique(Vars.begin(), Vars.end());
  }

  bool isAddressTaken(VarDecl *VD) {
    return std::binary_search(Vars.begin(), Vars.end(), VD);
  }
};
}


VarEquiv::VarEquiv(CFG *cfg /*, Stmt Body */) : cfg(cfg) {
}

void VarEquiv::analyze() {
   // Step 1: compute the set of address-taken variables, because we'll exclude those for now.
   // Step 2: find the set of interesting variables and number them.
}

void VarEquiv::setCurrentBlock(CFGBlock block) {
}

void VarEquiv::moveAfterNextStmt() {
}

void VarEquiv::dumpAll(const SourceManager& M) {
}

void VarEquiv::dumpCurrentStmt(const SourceManager& M) {
}

const VarDecl *VarEquiv::getRepresentative(const VarDecl *V) {
  return nullptr;
}


