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
// class efficiently.
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
  static const int Sentinel = -1;
  static const unsigned BaseCount = 4;

private:
  // We need to efficently map equivalence sets to unique representatives.
  // To do this, we assign a unique integer to each equivalence set and keep
  // the unique integers in a small range.

  // Wrapper type to keep set ids distinct from integers in the equivalent set.
  struct SetName {
    SetName(int Id) : Id(Id) {}
    int Id;
  };

  // Set Manager tracks the set names in use and the available set names.
  // It also tracks the number of unique integers in use.  If no set name
  // is available, it allocates a set name with the next higher available id.
  class SetManager {
  private:
    // List of set names, divided into those in use and those available for
    // re-use.
    SmallVector<SetName, BaseCount> SetNames;
    SmallVector<int, BaseCount> IdToSlot;
    int InUse;
  public:
    void freeSetName(SetName S) {
      assert(InUse >= 1);
      if (InUse > 1) {
        // Move the current name to the end of the list of set names
        // and decrement the number of names in the list that are in use.
        int Slot = IdToSlot[S.Id];
        assert(Slot >= 0 && Slot < InUse);
        SetNames[Slot] = SetNames[InUse - 1];
        IdToSlot[SetNames[Slot].Id] = Slot;
      }
      IdToSlot[S.Id] = Sentinel;
      InUse--;
    }

    SetName allocSetName() {
      if (InUse < SetNames.size()) {
        // If there is an available set name, use it.
        int NextSlot = InUse;
        SetName N = SetNames[NextSlot];
        IdToSlot[N.Id] = NextSlot;
        return N;
      } else {
        assert(InUse == SetNames.size());
        int NextSetId = InUse;
        SetName S(NextSetId);
        SetNames.push_back(S);
        IdToSlot[NextSetId] = NextSetId;
        InUse += 1;
        return S;
      }    
    }

    void getNewSplitSet(SetName S, SetName &NewSet) {
    }

    void resetSplitSets() {}
  };

  // We represent each equivalence class as an unordered doubly-linked list so 
  // that we can easily add/remove elements from the set. For efficiency, the
  // list nodes are allocated in a SmallVector and we use indices to
  // the slots in the SmallVector.  This avoids lots of small allocations
  // and frees as we operate on the equivalance sets.

  // Wrapper type to make sure that we don't confuse indices with the
  // integer members in equivalence sets.

  struct IndexType {
    int Index;
    IndexType(int I) : Index(I) {}
  };

  struct ListNode {    
    ListNode(Element E, SetName Name) : Elem(E), Set(Name) {
    }

    Element Elem;
    IndexType Prev;
    IndexType Next;
    SetName Set;
  };

  // Map an equivalence set element to its list node index.
  // Conceptually, this is just an array that maps elements to
  // list node indices, but we implement it in a more complicated
  // fashion to save space because the equivalence sets
  // may be sparse.

  class ElementMap {
  private:
    // This is implemented as a list that is searched linearly, provding space
    // efficiency.
    //
    // TODO: switch between several data strutures to provide a better trade-off 
    // between space usage and lookup efficiency as the number of elements grows.
    struct ElementMapNode {
      ElementMapNode(Element E, IndexType I) : Elem(E), Current(I) {
      }

      Element Elem;
      IndexType Current;
    };

    SmallVector<ElementMapNode, BaseCount> SmallMap;
  public:
    IndexType getIndex(Element Elem) {
      unsigned Count = SmallMap.size();
      for (unsigned i = 0; i < Count; i++) {
        if (SmallMap[i].Elem == Elem)
          return SmallMap[i].Current;
      }
      return Sentinel;
    }

    void addNewMapping(Element Elem, IndexType Index) {
      SmallMap.push_back(ElementMapNode(Elem, Index));
    }

    void setMapping(Element Elem, IndexType Index) {
      unsigned Count = SmallMap.size();
      for (unsigned i = 0; i < Count; i++) {
        if (SmallMap[i].Elem == Elem)
          SmallMap[i].Current = Index;
      }
    }

    void removeMapping(Element Elem) {
      unsigned Count = SmallMap.size();
      for (unsigned i = 0; i < Count; i++) {
        if (SmallMap[i].Elem == Elem) {
          // Copy the last element to the current element
          // and remove the last element.
          if (i < Count - 1)
            SmallMap[i] = SmallMap[Count - 1];
          SmallMap.pop_back();
        }
      }
    }
  };

  // The list nodes
  SmallVector<ListNode, BaseCount> Nodes;
  // Map an element to the index of its corresponding ListNode.
  ElementMap ElementToIndex;
  SetManager Sets;
  int Size;

public:
  PartitionRefinement(int Size) :Size(Size) {
  };

  // Add Elem to the set S for Member.  If Member does not have
  // a set, create a new set that contains Elem and Member.
  // It is an error of Elem already exists in another set.
  void add(Element Elem, Element Member) {
    assert(ElementToIndex.getIndex(Elem).Index == Sentinel);
    IndexType MemberIndex = ElementToIndex.getIndex(Member);
    if (MemberIndex.Index == Sentinel) {
       SetName S = Sets.allocSetName();
       int Next = Nodes.size();
       ListNode N1(Elem, S), N2(Member, S);
       IndexType N1Index(Next);  
       IndexType N2Index(Next + 1);
       N1.Prev = Sentinel;
       N1.Next = N2Index;
       N2.Prev = Next;
       N2.Next = Sentinel;
       Nodes.push_back(N1);
       Nodes.push_back(N2);
       ElementToIndex.addNewMapping(Elem, N1Index);
       ElementToIndex.addNewMapping(Member, N2Index);
    } else {
      ListNode &N1 = Nodes[MemberIndex];
      IndexType N2Index = Nodes.size() + 1;
      // insert new node for Elem after the
      // node for Member;
      ListNode N2(Elem, N1.Set);
      N2.Prev = MemberIndex;
      N2.Next = N1.Next;
      N1.Next = N2Index;
      Nodes.push_back(N2);
      ElementToIndex.addNewMapping(Elem, N2Index);
    }
  }

private:
  static bool isLastNode(ListNode &Node) {
    return Node.Next.Index == Sentinel;
  }

  static bool isFirstNode(ListNode &Node) {
    return Node.Prev.Index == Sentinel;
  }

  static bool isSingleton(ListNode &Node) {
    return isFirstNode(Node) && isLastNode(Node);
  }

  int removeHelper(IndexType NodeIndex) {
    ListNode &Node = Nodes[NodeIndex.Index];
    if (Nodes.size() >= 2) {
      ListNode LastNode = Nodes[Nodes.size() - 1];
      Node = LastNode;
      ElementToIndex.removeMapping(Node.Elem);
      ElementToIndex.setMapping(LastNode.Elem, NodeIndex);
      if (LastNode.Prev.Index != Sentinel) {
        Nodes[LastNode.Prev.Index].Next = NodeIndex;
      }
      if (LastNode.Next.Index != Sentinel) {
        Nodes[LastNode.Next.Index].Prev = NodeIndex;
      }
    }
    Nodes.pop_back();
  }

public:
  bool isMember(Element Elem) {
    return ElementToIndex.getIndex(Elem).Index != Sentinel;
  }

  // Remove Element from its current set.
  void remove(Element Elem) {
    IndexType MemberIndex = ElementToIndex.getIndex(Elem);
    if (MemberIndex.Index != Sentinel) {
      ListNode Node = Nodes[MemberIndex.Index];
      IndexType Prev = Node.Prev;
      IndexType Next = Node.Next;
      if (Prev.Index != Sentinel)
        Nodes[Prev.Index].Next = Node.Next;
      if (Node.Next.Index != Sentinel)
        Nodes[Next.Index].Prev = Node.Prev;\
      removeHelper(MemberIndex);
      if (isSingleton(Node))
        Sets.freeSetName(Node.Set);
    }
  }

private:
  void refineSet(ListNode &S) {
  }

  void refine(PartitionRefinement R) {
    // Check that all elements of the current equivalence classes
    // are members of at least one equivalence class in R.  Elements
    // not in any equivalence class in R are in singleton equivalence
    // classes.  We could represent them, but we just delete them
    // now to save time.
    int i = 0;
    while (i < Nodes.size()) {
      // Don't increment if we remove an element, as the
      // last element in the list of nodes will replace the
      // removed element.
      Element Elem = Nodes[i].Elem;
      if (R.isMember(Elem))
        i++;
      else removeHelper(Elem);
    }

    for (i = 0; i < R.Nodes.size(); i++)
      if (isFirstNode(R.Nodes[i]))
        refineSet(R.Nodes[i]);
  }

  // If Elem is a member of a set, return the representative
  // element.  Otherwise return the Sentinel value.
  Element getRepresentative(Element Elem) {
    IndexType MemberIndex = getIndex(Elem);
    if (MemberIndex.Index == Sentinel)
      return Sentinel;
    ListNode &Node = Nodes[MemberIndex];
    while (Node.Prev.Index != Sentinel)
      Node = Nodes[Node.Prev];
    return Node.Elem;
  }

  // Dump the set that elem is equivalent to.
  void Dump(raw_ostream &OS, Element Elem) {
    IndexType MemberIndex = ElementToIndex.getIndex(Elem);
    OS << Elem;
    OS << ": ";
    if (MemberIndex.Index == Sentinel) {
      OS << "Nothing";
      return;
    }
    ListNode &Node = Nodes[MemberIndex];
    while (Node.Prev.Index != Sentinel)
      Node = Nodes[Node.Prev];
    OS << "{";
    OS << Node.Elem;
    while (Node.Next.Index != Sentinel) {
      Node = Nodes[Node.Next];
      OS << ", ";
      OS << Node.Elem;
    }
    OS << "}\n";;
  }

  void Dump(raw_ostream &OS) {
    unsigned Count = Nodes.size();
    if (Count == 0)
      OS << "Equivalence classes are empty\n";
    else
      OS << "Equivalence classes:\n";
      
    for (unsigned i = 0; i < Count; i++) {
      ListNode &Node = Nodes[i];
      if (Node.Prev.Index == Sentinel)
        Dump(OS, Node.Elem);
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


