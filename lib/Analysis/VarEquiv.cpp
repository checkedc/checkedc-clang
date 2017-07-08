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
// Represent a set of equivalence classes.
class EquivalenceClasses {
public:
  typedef int Element;
  static const int Sentinel = -1;

private:
  typedef int Index;
  typedef int SetId;

  static const unsigned BaseCount = 4;

  struct ListNode {    
    ListNode(Element E) : Elem(E) {
    }

    Element Elem;
    Index Prev;
    Index Next;
  };

  struct ElementMapNode {
    ElementMapNode(Element E, Index I) : Elem(E), Current(I) {
    }

    Element Elem;
    Index Current;
  };

  // Each set is represented as a doubly-linked list.  The
  // list nodes are stored in Nodes.
  SmallVector<ListNode, BaseCount> Nodes;
  // Map an element to the index of its corresponding ListNode.
  //
  // This is implemented as a list that is searched linearly, provding space
  // efficiency.  For lookup efficiency, we could use an array.  TODO: use
  // several data strutures to provide a better trade-off between space usage
  // and lookup efficiency as the number of elements grows.
  SmallVector<ElementMapNode, BaseCount> SmallMap;
  int Size;

  Index getListNodeIndex(Element Elem) { 
    unsigned Count = SmallMap.size();
    for (unsigned i = 0; i < Count; i++) {
       if (SmallMap[i].Elem == Elem)
         return SmallMap[i].Current;
    }
    return Sentinel;
  }

  void addMapping(Element Elem, int Index) {
    SmallMap.push_back(ElementMapNode(Elem, Index));
  }

  void removeMapping(Element Elem) {
    unsigned Count = SmallMap.size();
    for (unsigned i = 0; i < Count; i++) {
      if (SmallMap[i].Elem == Elem) {
        // Copy the last element to the current element
        // and remove the last element.
        if (i < Count-1)
          SmallMap[i] = SmallMap[Count - 1];
        SmallMap.pop_back();
      }
    }
  }
   
public:
  EquivalenceClasses(int Size) :Size(Size) {
  };

  // Add Elem to the set S for Member.  If Member does not have
  // a set, create a new set that contains Elem and Member.
  // It is an error of Elem already exists in another set.
  void add(Element Elem, Element Member) {
    assert(getListNodeIndex(Elem) == Sentinel);
    Index MemberIndex = getListNodeIndex(Member);
    if (MemberIndex == Sentinel) {
       int Next = Nodes.size();
       ListNode N1(Elem), N2(Member);;
       Index N1Index = Next;
       Index N2Index = Next + 1;
       N1.Prev = Sentinel;
       N1.Next = N2Index;
       N2.Prev = Next;
       N2.Next = Sentinel;
       Nodes.push_back(N1);
       Nodes.push_back(N2);
       addMapping(Elem, N1Index);
       addMapping(Member, N2Index);
    } else {
      ListNode &N1 = Nodes[MemberIndex];
      Index N2Index = Nodes.size() + 1;
      // insert new node for Elem after the
      // node for Member;
      ListNode N2(Elem);
      N2.Prev = MemberIndex;
      N2.Next = N1.Next;
      N1.Next = N2Index;
      Nodes.push_back(N2);
      addMapping(Elem, N2Index);
    }
  }

  // Remove Element from its current set.
  void remove(Element Elem) {
    Index MemberIndex = getListNodeIndex(Elem);
    if (MemberIndex != Sentinel) {
      ListNode &Node = Nodes[MemberIndex];
      if (Node.Prev != Sentinel)
        Nodes[Node.Prev].Next = Node.Next;
      if (Node.Next != Sentinel)
        Nodes[Node.Next].Prev = Node.Prev;
      removeMapping(Elem);
    }
  }

  // If Elem is a member of a set, return the representative
  // element.  Otherwise return the Sentinel value.
  Element getRepresentative(Element Elem) {
    Index MemberIndex = getListNodeIndex(Elem);
    if (MemberIndex == Sentinel)
      return Sentinel;
    ListNode &Node = Nodes[MemberIndex];
    while (Node.Prev != Sentinel)
      Node = Nodes[Node.Prev];
    return Node.Elem;
  }

  // Dump the set that elem is equivalent to.
  void Dump(raw_ostream &OS, Element Elem) {
    Index MemberIndex = getListNodeIndex(Elem);
    OS << Elem;
    OS << ": ";
    if (MemberIndex == Sentinel) {
      OS << "Nothing";
      return;
    }
    ListNode &Node = Nodes[MemberIndex];
    while (Node.Prev != Sentinel)
      Node = Nodes[Node.Prev];
    OS << "{";
    OS << Node.Elem;
    while (Node.Next != Sentinel) {
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
      if (Node.Prev == Sentinel)
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


