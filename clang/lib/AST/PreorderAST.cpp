//===------ PreorderAST.cpp: An n-ary preorder abstract syntax tree -------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements methods to create and manipulate an n-ary preorder
//  abstract syntax tree which is used to semantically compare two expressions.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/PreorderAST.h"

using namespace clang;

void PreorderAST::AttachNode(Node *N, Node *Parent) {
  // A LeafExprNode cannot be the parent of any node.
  if (Parent && isa<LeafExprNode>(Parent)) {
    assert(0 && "Attempting to add a node to a LeafExprNode");
    SetError();
    return;
  }

  // If the root is null, make the current node the root.
  if (!Root) {
    if (!isa<BinaryOperatorNode>(N)) {
      assert(0 && "The root of a PreorderAST must be a BinaryOperatorNode");
      SetError();
      return;
    }
    if (Parent) {
      assert(0 && "Parent node must be null if the PreorderAST root is null");
      SetError();
      return;
    }
    Root = N;
  }

  // Add the current node to the list of children of its parent.
  if (auto *B = dyn_cast_or_null<BinaryOperatorNode>(Parent))
    B->Children.push_back(N);
  // Set the current node as the child of its parent.
  else if (auto *U = dyn_cast_or_null<UnaryOperatorNode>(Parent))
    U->Child = N;
  // Set the current node as the base of its parent.
  else if (auto *M = dyn_cast_or_null<MemberNode>(Parent))
    M->Base = N;
  // Set the current node as the child of its parent.
  else if (auto *I = dyn_cast_or_null<ImplicitCastNode>(Parent))
    I->Child = N;
}

void PreorderAST::Create(Expr *E, Node *Parent) {
  if (!E)
    return;

  E = Lex.IgnoreValuePreservingOperations(Ctx, E->IgnoreParens());

  if (!Root) {
    // The invariant is that the root node must be a BinaryOperatorNode with an
    // addition operator. So for expressions like "if (*p)", we don't have a
    // BinaryOperator. So when we enter this function there is no root and the
    // Root node is null. So we create a new BinaryOperatorNode with + as the
    // operator and add 0 as a LeafExprNode child of this BinaryOperatorNode.
    // This helps us compare expressions like "p" and "p + 1" by normalizing
    // "p" to "p + 0".

    AddZero(E, Parent);

  } else if (auto *BO = dyn_cast<BinaryOperator>(E)) {
    CreateBinaryOperator(BO, Parent);

  } else if (auto *UO = dyn_cast<UnaryOperator>(E)) {
    CreateUnaryOperator(UO, Parent);

  } else if (auto *AE = dyn_cast<ArraySubscriptExpr>(E)) {
    CreateArraySubscript(AE, Parent);

  } else if (auto *ME = dyn_cast<MemberExpr>(E)) {
    CreateMember(ME, Parent);

  } else if (auto *ICE = dyn_cast<ImplicitCastExpr>(E)) {
    CreateImplicitCast(ICE, Parent);

  } else {
    auto *N = new LeafExprNode(E, Parent);
    AttachNode(N, Parent);
  }
}

void PreorderAST::CreateBinaryOperator(BinaryOperator *E, Node *Parent) {
  BinaryOperatorKind BinOp = E->getOpcode();
  Expr *LHS = E->getLHS();
  Expr *RHS = E->getRHS();

  // We can convert (e1 - e2) to (e1 + -e2) if -e2 does not overflow.  One
  // instance where -e2 can overflow is if e2 is INT_MIN. Here, instead of
  // specifically checking whether e2 is INT_MIN, we add a unary minus to e2
  // and then check if the resultant expression -e2 overflows. If it
  // overflows, we undo the unary minus operator.

  // TODO: Currently, we can only prove that integer constant expressions do
  // not overflow. We still need to handle proving that non-constant
  // expressions do not overflow.
  if (BinOp == BO_Sub && RHS->isIntegerConstantExpr(Ctx)) {
    Expr *UOMinusRHS =
      UnaryOperator::Create(Ctx, RHS, UO_Minus, RHS->getType(),
                            RHS->getValueKind(), RHS->getObjectKind(),
                            SourceLocation(), /*CanOverflow*/ true,
                            FPOptionsOverride());

    SmallVector<PartialDiagnosticAt, 8> Diag;
    UOMinusRHS->EvaluateKnownConstIntCheckOverflow(Ctx, &Diag);

    bool Overflow = false;
    for (auto &PD : Diag) {
      if (PD.second.getDiagID() == diag::note_constexpr_overflow) {
        Overflow = true;
        break;
      }
    }

    if (!Overflow) {
      BinOp = BO_Add;
      RHS = UOMinusRHS;
    }

    // TODO: In case of overflow we leak the memory allocated to UOMinusRHS.
    // Whereas if there is no overflow we leak the memory initially allocated
    // to RHS.
  }

  auto *N = new BinaryOperatorNode(BinOp, Parent);
  AttachNode(N, Parent);

  Create(LHS, /*Parent*/ N);
  Create(RHS, /*Parent*/ N);
}

void PreorderAST::CreateUnaryOperator(UnaryOperator *E, Node *Parent) {
  UnaryOperatorKind Op = E->getOpcode();
  if (Op == UnaryOperatorKind::UO_Deref) {
    // The child of a dereference operator must be a binary operator so that
    // *e and *(e + 0) have the same canonical form. So for an expression of
    // the form *e, we create a UnaryOperatorNode whose child is a
    // BinaryOperatorNode e + 0.
    auto *N = new UnaryOperatorNode(UnaryOperatorKind::UO_Deref, Parent);
    AttachNode(N, Parent);
    AddZero(E->getSubExpr(), /*Parent*/ N);
  } else if ((Op == UnaryOperatorKind::UO_Plus ||
              Op == UnaryOperatorKind::UO_Minus) &&
              E->isIntegerConstantExpr(Ctx)) {
    // For integer constant expressions of the form +e or -e, we create a
    // LeafExprNode rather than a UnaryOperatorNode so that these expressions
    // can be constant folded. Constant folding only folds LeafExprNodes that
    // are children of a BinaryOperatorNode.
    auto *N = new LeafExprNode(E, Parent);
    AttachNode(N, Parent);
  } else {
    auto *N = new UnaryOperatorNode(Op, Parent);
    AttachNode(N, Parent);
    Create(E->getSubExpr(), /*Parent*/ N);
  }
}

void PreorderAST::CreateArraySubscript(ArraySubscriptExpr *E, Node *Parent) {
  // e1[e2] has the same canonical form as *(e1 + e2 + 0).
  auto *DerefExpr = BinaryOperator::Create(Ctx, E->getBase(), E->getIdx(),
                                           BinaryOperatorKind::BO_Add, E->getType(),
                                           E->getValueKind(), E->getObjectKind(),
                                           E->getExprLoc(), FPOptionsOverride());
  auto *N = new UnaryOperatorNode(UnaryOperatorKind::UO_Deref, Parent);
  AttachNode(N, Parent);
  AddZero(DerefExpr, N);
}

void PreorderAST::CreateMember(MemberExpr *E, Node *Parent) {
  Expr *Base = Lex.IgnoreValuePreservingOperations(Ctx, E->getBase()->IgnoreParens());
  ValueDecl *Field = E->getMemberDecl();

  // Expressions such as a->f, (*a).f, and a[0].f should have the same
  // canonical form: a MemberNode with a Base node of a + 0 and a Field
  // of f. Here, we determine whether the expression E is of one of the
  // forms a->f, (*a).f, etc. and create the base expression a.
  Expr *ArrowBase = nullptr;
  if (E->isArrow()) {
    ArrowBase = Base;
  } else {
    if (const auto *UO = dyn_cast<UnaryOperator>(Base)) {
      if (UO->getOpcode() == UnaryOperatorKind::UO_Deref)
        ArrowBase = UO->getSubExpr();
    } else if (auto *AE = dyn_cast<ArraySubscriptExpr>(Base)) {
      ArrowBase = BinaryOperator::Create(Ctx, AE->getBase(), AE->getIdx(),
                                         BinaryOperatorKind::BO_Add, AE->getType(),
                                         AE->getValueKind(), AE->getObjectKind(),
                                         AE->getExprLoc(), FPOptionsOverride());
    }
  }

  if (ArrowBase) {
    // If ArrowBase exists, then E is of the form ArrowBase->f,
    // (*ArrowBase).f, etc. The Base of the MemberNode is ArrowBase + 0
    // so that expressions such as a->f, (*a).f, (a + 0)->f, and a[0].f
    // all have the same canonical form.
    auto *N = new MemberNode(Field, /*IsArrow*/ true, Parent);
    AttachNode(N, Parent);
    AddZero(ArrowBase, /*Parent*/ N);
  } else {
    // If no ArrowBase exists, then E is of the form a.f.
    auto *N = new MemberNode(Field, /*IsArrow*/ false, Parent);
    AttachNode(N, Parent);
    Create(Base, /*Parent*/ N);
  }
}

void PreorderAST::CreateImplicitCast(ImplicitCastExpr *E, Node *Parent) {
  auto *N = new ImplicitCastNode(E->getCastKind(), Parent);
  AttachNode(N, Parent);
  Create(E->getSubExpr(), /*Parent*/ N);
}

void PreorderAST::AddZero(Expr *E, Node *Parent) {
  auto *N = new BinaryOperatorNode(BO_Add, Parent);
  AttachNode(N, Parent);

  llvm::APInt Zero(Ctx.getTargetInfo().getIntWidth(), 0);
  auto *ZeroLiteral = new (Ctx) IntegerLiteral(Ctx, Zero, Ctx.IntTy,
                                               SourceLocation());
  auto *L = new LeafExprNode(ZeroLiteral, N);
  AttachNode(L, /*Parent*/ N);
  Create(E, /*Parent*/ N);
}

bool BinaryOperatorNode::CanCoalesce() {
  // We can only coalesce if the operator of the current and parent node is
  // commutative and associative. This is because after coalescing we later
  // need to sort the nodes and if the operator is not commutative and
  // associative then sorting would be incorrect.
  if (!IsOpCommutativeAndAssociative())
    return false;
  auto *BParent = dyn_cast_or_null<BinaryOperatorNode>(Parent);
  if (!BParent || !BParent->IsOpCommutativeAndAssociative())
    return false;

  // We can coalesce in the following scenarios:
  // 1. The current and parent nodes have the same operator OR
  // 2. The current node is the only child of its operator node (maybe as a
  // result of constant folding).
  return Opc == BParent->Opc || Children.size() == 1;
}

bool BinaryOperatorNode::Coalesce(bool &Error) {
  if (Error)
    return false;

  // Coalesce the children first.
  // Since Children is modified within the loop, we need to evaluate
  // the loop end on each iteration.
  size_t I = 0;
  while (I != Children.size()) {
    auto *Child = Children[I];
    bool ChildCoalesced = Child->Coalesce(Error);
    // If Child was not coalesced into this node, then we can increment I
    // in order to coalesce the next child node. Otherwise, if Child was
    // coalesced into this node, then Children[I] still needs to be coalesced.
    if (!ChildCoalesced)
      ++I;
  }

  if (!CanCoalesce())
    return false;

  // If the current node can be coalesced, its parent must be a
  // BinaryOperatorNode.
  auto *BParent = dyn_cast_or_null<BinaryOperatorNode>(Parent);
  if (!BParent)
    return false;

  // Remove the current node from the list of children of its parent.
  for (auto I = BParent->Children.begin(), E = BParent->Children.end(); I != E; ++I) {
    if (*I == this) {
      BParent->Children.erase(I);
      break;
    }
  }

  // Move all children of the current node to its parent.
  for (auto *Child : Children) {
    Child->Parent = BParent;
    BParent->Children.push_back(Child);
  }

  // Delete the current node.
  delete this;

  // The current node was coalesced into its parent.
  return true;
}

bool UnaryOperatorNode::Coalesce(bool &Error) {
  if (Error)
    return false;
  Child->Coalesce(Error);
  return false;
}

bool MemberNode::Coalesce(bool &Error) {
  if (Error)
    return false;
  Base->Coalesce(Error);
  return false;
}

bool ImplicitCastNode::Coalesce(bool &Error) {
  if (Error)
    return false;
  Child->Coalesce(Error);
  return false;
}

bool LeafExprNode::Coalesce(bool &Error) {
  return false;
}

void BinaryOperatorNode::Sort(Lexicographic Lex) {
  // Sort the children first.
  for (auto *Child : Children)
    Child->Sort(Lex);

  // We can only sort if the operator is commutative and associative.
  if (!IsOpCommutativeAndAssociative())
    return;

  // Sort the children.
  llvm::sort(Children.begin(), Children.end(),
            [&](const Node *N1, const Node *N2) {
              return N1->Compare(N2, Lex) == Result::LessThan;
            });
}

void UnaryOperatorNode::Sort(Lexicographic Lex) {
  Child->Sort(Lex);
}

void MemberNode::Sort(Lexicographic Lex) {
  Base->Sort(Lex);
}

void ImplicitCastNode::Sort(Lexicographic Lex) {
  Child->Sort(Lex);
}

void LeafExprNode::Sort(Lexicographic Lex) { }

bool BinaryOperatorNode::ConstantFold(bool &Error, ASTContext &Ctx) {
  if (Error)
    return false;

  size_t ConstStartIdx = 0;
  unsigned NumConsts = 0;
  llvm::APSInt ConstFoldedVal;

  size_t Idx = 0;
  while (Idx != Children.size()) {
    auto *Child = Children[Idx];

    // Recursively constant fold the non-leaf children of a BinaryOperatorNode.
    if (!isa<LeafExprNode>(Child)) {
      bool ChildDeleted = Child->ConstantFold(Error, Ctx);
      // If Child was not deleted during constant folding, then we can
      // increment Idx in order to process the next child node. Otherwise,
      // if Child was deleted, then Children[Idx] still needs to be processed.
      if (!ChildDeleted)
        ++Idx;
      continue;
    }

    ++Idx;

    // We can only constant fold if the operator is commutative and
    // associative.
    if (!IsOpCommutativeAndAssociative())
      continue;

    auto *ChildLeafNode = dyn_cast_or_null<LeafExprNode>(Child);
    if (!ChildLeafNode)
      continue;

    // Check if the child node is an integer constant.
    Optional<llvm::APSInt> OptCurrConstVal =
          ChildLeafNode->E->getIntegerConstantExpr(Ctx);
    if (!OptCurrConstVal)
      continue;

    llvm::APSInt CurrConstVal = *OptCurrConstVal;
    ++NumConsts;

    if (NumConsts == 1) {
      // We will use ConstStartIdx later in this function to delete the
      // constant folded nodes.
      ConstStartIdx = Idx - 1;
      ConstFoldedVal = CurrConstVal;

    } else {
      // Ensure that ConstFoldedVal and CurrConstVal have the same bit width.
      if (ConstFoldedVal.getBitWidth() < CurrConstVal.getBitWidth())
        ConstFoldedVal = ConstFoldedVal.extOrTrunc(CurrConstVal.getBitWidth());
      else if (CurrConstVal.getBitWidth() < ConstFoldedVal.getBitWidth())
        CurrConstVal = CurrConstVal.extOrTrunc(ConstFoldedVal.getBitWidth());

      // Constant fold based on the operator.
      bool Overflow;
      switch(Opc) {
        default: continue;
        case BO_Add:
          ConstFoldedVal = ConstFoldedVal.sadd_ov(CurrConstVal, Overflow);
          break;
        case BO_Mul:
          ConstFoldedVal = ConstFoldedVal.smul_ov(CurrConstVal, Overflow);
          break;
      }

      // If we encounter an overflow during constant folding we cannot proceed.
      if (Overflow) {
        Error = true;
        return false;
      }
    }
  }

  // To fold constants we need at least 1 constant. If we have only 1 constant
  // it can trivially be folded. Folding 1 constant allows us to constant
  // fold expressions such as *(p + -(1 + 2)) to *(p + -3).
  if (NumConsts < 1)
    return false;

  // Delete the folded constants and reclaim memory.
  // Note: We do not explicitly need to increment the iterator because after
  // erase the iterator automatically points to the new location of the element
  // following the one we just erased.
  llvm::SmallVector<Node *, 2>::iterator I =
    Children.begin() + ConstStartIdx;
  while (NumConsts--) {
    delete(*I);
    Children.erase(I);
  }

  llvm::APInt IntVal(Ctx.getTargetInfo().getIntWidth(),
                     ConstFoldedVal.getLimitedValue());

  Expr *ConstFoldedExpr = new (Ctx) IntegerLiteral(Ctx, IntVal, Ctx.IntTy,
                                                   SourceLocation());

  // Add the constant folded expression to list of children of the current
  // BinaryOperatorNode.
  Children.push_back(new LeafExprNode(ConstFoldedExpr, this));

  // If the constant folded expr is the only child of this BinaryOperatorNode
  // we can coalesce the node. This node may be deleted during coalescing.
  if (Children.size() == 1 && CanCoalesce())
    return Coalesce(Error);

  return false;
}

bool UnaryOperatorNode::ConstantFold(bool &Error, ASTContext &Ctx) {
  if (Error)
    return false;
  Child->ConstantFold(Error, Ctx);
  return false;
}

bool MemberNode::ConstantFold(bool &Error, ASTContext &Ctx) {
  if (Error)
    return false;
  Base->ConstantFold(Error, Ctx);
  return false;
}

bool ImplicitCastNode::ConstantFold(bool &Error, ASTContext &Ctx) {
  if (Error)
    return false;
  Child->ConstantFold(Error, Ctx);
  return false;
}

bool LeafExprNode::ConstantFold(bool &Error, ASTContext &Ctx) {
  return false;
}

// TODO: Remove this method after the updated implementation of the bounds
// widening analysis merges. This method will be replaced by
// PreorderAST::GetExprIntDiff. See issue
// https://github.com/microsoft/checkedc-clang/issues/1078.
bool PreorderAST::GetDerefOffset(Node *UpperNode, Node *DerefNode,
				 llvm::APSInt &Offset) {
  // Extract the offset by which a pointer is dereferenced. For the pointer we
  // compare the dereference expr with the declared upper bound expr. If the
  // non-integer parts of the two exprs are not equal we say that a valid
  // offset does not exist and return false. If the non-integer parts of the
  // two exprs are equal the offset is calculated as:
  // (integer part of deref expr - integer part of upper bound expr).

  // Since we have already normalized exprs like "*p" to "*(p + 0)" we require
  // that the root of the preorder AST is a BinaryOperatorNode.
  auto *B1 = dyn_cast_or_null<BinaryOperatorNode>(UpperNode);
  auto *B2 = dyn_cast_or_null<BinaryOperatorNode>(DerefNode);

  if (!B1 || !B2)
    return false;

  // If the opcodes mismatch we cannot have a valid offset.
  if (B1->Opc != B2->Opc)
    return false;

  // We have already constant folded the constants. So return false if the
  // number of children mismatch.
  if (B1->Children.size() != B2->Children.size())
    return false;

  // Check if the children are equivalent.
  for (size_t I = 0; I != B1->Children.size(); ++I) {
    auto *Child1 = B1->Children[I];
    auto *Child2 = B2->Children[I];

    if (Child1->Compare(Child2, Lex) == Result::Equal)
      continue;

    // If the children are not equal we require that they be integer constant
    // leaf nodes. Otherwise we cannot have a valid offset.
    auto *L1 = dyn_cast_or_null<LeafExprNode>(Child1);
    auto *L2 = dyn_cast_or_null<LeafExprNode>(Child2);

    if (!L1 || !L2)
      return false;

    // Return false if either of the leaf nodes is not an integer constant.
    Optional<llvm::APSInt> OptUpperOffset =
                           L1->E->getIntegerConstantExpr(Ctx);
    if (!OptUpperOffset)
      return false;

    Optional<llvm::APSInt> OptDerefOffset =
                           L2->E->getIntegerConstantExpr(Ctx);
    if (!OptDerefOffset)
      return false;

    // Offset should always be of the form (ptr + offset). So we check for
    // addition.
    // Note: We have already converted (ptr - offset) to (ptr + -offset). So
    // it is okay to only check for addition.
    if (B1->Opc != BO_Add)
      return false;

    // This guards us from a case where the constants were not folded for
    // some reason. In theory this should never happen. But we are adding this
    // check just in case.
    llvm::APSInt Zero(Ctx.getTargetInfo().getIntWidth(), 0);
    if (llvm::APSInt::compareValues(Offset, Zero) != 0)
      return false;

    // offset = deref offset - declared upper bound offset.
    // Return false if we encounter an overflow.
    bool Overflow;
    Offset = (*OptDerefOffset).ssub_ov(*OptUpperOffset, Overflow);
    if (Overflow)
      return false;
  }

  return true;
}

bool PreorderAST::GetExprIntDiff(Node *E1, Node *E2, llvm::APSInt &Offset) {
  // Get the integer difference between expressions.

  // If E1 and E2 are not comparable, return false.
  // Else perform E1 - E2, store the integer result in Offset and return true.

  // E1 and E2 are not comparable if their non-integer parts are not equal.

  // Since we have already normalized exprs like "*p" to "*(p + 0)" we require
  // that the root of the preorder AST is a BinaryOperatorNode.
  auto *B1 = dyn_cast_or_null<BinaryOperatorNode>(E1);
  auto *B2 = dyn_cast_or_null<BinaryOperatorNode>(E2);

  if (!B1 || !B2)
    return false;

  // If the opcodes mismatch we cannot have a valid offset.
  if (B1->Opc != B2->Opc)
    return false;

  // Offset should always be of the form (ptr + offset). So we check for
  // addition.
  // Note: We have already converted (ptr - offset) to (ptr + -offset). So
  // it is okay to only check for addition.
  if (B1->Opc != BO_Add)
    return false;

  // We have already constant folded the constants. So return false if the
  // number of children mismatch.
  if (B1->Children.size() != B2->Children.size())
      return false;

  llvm::APSInt Zero(Ctx.getTargetInfo().getIntWidth(), 0);
  // Initialize Offset to 0.
  Offset = Zero;

  // Check if the children are equivalent.
  for (size_t I = 0; I != B1->Children.size(); ++I) {
    auto *Child1 = B1->Children[I];
    auto *Child2 = B2->Children[I];

    if (Child1->Compare(Child2, Lex) == Result::Equal)
      continue;

    // If the children are not equal we require that they be integer constant
    // leaf nodes. Otherwise we cannot have a valid offset.
    auto *L1 = dyn_cast_or_null<LeafExprNode>(Child1);
    auto *L2 = dyn_cast_or_null<LeafExprNode>(Child2);

    if (!L1 || !L2)
      return false;

    // Return false if either of the leaf nodes is not an integer constant.
    Optional<llvm::APSInt> OptIntegerPart1;
    if (!(OptIntegerPart1 = L1->E->getIntegerConstantExpr(Ctx)))
      return false;

    Optional<llvm::APSInt> OptIntegerPart2;
    if (!(OptIntegerPart2 = L2->E->getIntegerConstantExpr(Ctx)))
      return false;

    // This guards us from a case where the constants were not folded for
    // some reason. In theory this should never happen. But we are adding this
    // check just in case.
    if (llvm::APSInt::compareValues(Offset, Zero) != 0)
      return false;

    // Offset = IntegerPart1 - IntegerPart2.
    // Return false if we encounter an overflow.
    bool Overflow;
    Offset = (*OptIntegerPart1).ssub_ov(*OptIntegerPart2, Overflow);
    if (Overflow)
      return false;
  }

  return true;
}

Result BinaryOperatorNode::Compare(const Node *Other, Lexicographic Lex) const {
  Result KindComparison = CompareKinds(Other);
  if (KindComparison != Result::Equal)
    return KindComparison;

  const BinaryOperatorNode *B = dyn_cast<BinaryOperatorNode>(Other);
  if (!B)
    return Result::LessThan;

  // If the Opcodes mismatch.
  if (Opc < B->Opc)
    return Result::LessThan;
  if (Opc > B->Opc)
    return Result::GreaterThan;

  size_t ChildCount1 = Children.size(),
         ChildCount2 = B->Children.size();

  // If the number of children of the two nodes mismatch.
  if (ChildCount1 < ChildCount2)
    return Result::LessThan;
  if (ChildCount1 > ChildCount2)
    return Result::GreaterThan;

  // Match each child of the two nodes.
  for (size_t I = 0; I != ChildCount1; ++I) {
    auto *Child1 = Children[I];
    auto *Child2 = B->Children[I];

    Result ChildComparison = Child1->Compare(Child2, Lex);

    // If any child differs between the two nodes.
    if (ChildComparison != Result::Equal)
      return ChildComparison;
  }
  return Result::Equal;
}

Result UnaryOperatorNode::Compare(const Node *Other, Lexicographic Lex) const {
  Result KindComparison = CompareKinds(Other);
  if (KindComparison != Result::Equal)
    return KindComparison;

  const UnaryOperatorNode *U = dyn_cast<UnaryOperatorNode>(Other);
  if (!U)
    return Result::LessThan;

  // If the Opcodes mismatch.
  if (Opc < U->Opc)
    return Result::LessThan;
  if (Opc > U->Opc)
    return Result::GreaterThan;

  return Child->Compare(U->Child, Lex);
}

Result MemberNode::Compare(const Node *Other, Lexicographic Lex) const {
  Result KindComparison = CompareKinds(Other);
  if (KindComparison != Result::Equal)
    return KindComparison;

  const MemberNode *M = dyn_cast<MemberNode>(Other);
  if (!M)
    return Result::LessThan;

  // If the arrow flags mismatch.
  if (IsArrow && !M->IsArrow)
    return Result::LessThan;
  if (!IsArrow && M->IsArrow)
    return Result::GreaterThan;

  // If the fields mismatch.
  Result FieldCompare = Lex.CompareDecl(Field, M->Field);
  if (FieldCompare != Result::Equal)
    return FieldCompare;

  return Base->Compare(M->Base, Lex);
}

Result ImplicitCastNode::Compare(const Node *Other, Lexicographic Lex) const {
  Result KindComparison = CompareKinds(Other);
  if (KindComparison != Result::Equal)
    return KindComparison;

  const ImplicitCastNode *I = dyn_cast<ImplicitCastNode>(Other);
  if (!I)
    return Result::LessThan;

  // If the cast kinds mismatch.
  if (CK < I->CK)
    return Result::LessThan;
  if (CK > I->CK)
    return Result::GreaterThan;

  return Child->Compare(I->Child, Lex);
}

Result LeafExprNode::Compare(const Node *Other, Lexicographic Lex) const {
  Result KindComparison = CompareKinds(Other);
  if (KindComparison != Result::Equal)
    return KindComparison;

  const LeafExprNode *L = dyn_cast<LeafExprNode>(Other);
  if (!L)
    return Result::LessThan;

  // Compare the exprs for two leaf nodes.
  return Lex.CompareExpr(E, L->E);
}

void PreorderAST::Normalize() {
  // TODO: Perform simple arithmetic optimizations/transformations on the
  // constants in the nodes.

  // We only need one call to Coalesce, Sort and ConstantFold in order to
  // normalize the tree, since:
  // 1. For any Node N, calling N->Coalesce fully coalesces N and all children
  //    of N.
  // 2. For any Node N, calling N->Sort fully sorts N and all children of N.
  // 3. For any Node N, calling N->ConstantFold fully constant folds N and all
  //    children of N.
  // 4. After calling Sort, there is no further coalescing to be done, since
  //    Sort creates no new nodes.
  // 5. After calling ConstantFold, there is no further coalescing to be done,
  //    since ConstantFold does not create any new BinaryOperatorNodes. At
  //    most, ConstantFold may create new LeafExprNodes.
  // 6. After calling ConstantFold, there is no further sorting to be done,
  //    since ConstantFold adds any newly created LeafExprNodes to the end of
  //    the Children list. This may break sorting only among the constant
  //    child nodes. The child nodes are sorted correctly when the Parent node
  //    of the Children list is constant folded.
  Root->Coalesce(Error);
  if (!Error) {
    Root->Sort(Lex);
    Root->ConstantFold(Error, Ctx);
  }

  if (Ctx.getLangOpts().DumpPreorderAST) {
    Root->PrettyPrint(OS, Ctx);
    OS << "--------------------------------------\n";
  }
}

void BinaryOperatorNode::PrettyPrint(llvm::raw_ostream &OS,
                                     ASTContext &Ctx) const {
  OS << BinaryOperator::getOpcodeStr(Opc) << "\n";
  for (auto *Child : Children)
    Child->PrettyPrint(OS, Ctx);
}

void UnaryOperatorNode::PrettyPrint(llvm::raw_ostream &OS,
                                    ASTContext &Ctx) const {
  OS << UnaryOperator::getOpcodeStr(Opc) << "\n";
  Child->PrettyPrint(OS, Ctx);
}

void MemberNode::PrettyPrint(llvm::raw_ostream &OS,
                             ASTContext &Ctx) const {
  if (IsArrow)
    OS << "->\n";
  else
    OS << ".\n";
  Base->PrettyPrint(OS, Ctx);
  Field->dump(OS);
}

void ImplicitCastNode::PrettyPrint(llvm::raw_ostream &OS,
                                   ASTContext &Ctx) const {
  OS << CastExpr::getCastKindName(CK) << "\n";
  Child->PrettyPrint(OS, Ctx);
}

void LeafExprNode::PrettyPrint(llvm::raw_ostream &OS,
                               ASTContext &Ctx) const {
  E->dump(OS, Ctx);
}

void BinaryOperatorNode::Cleanup() {
  for (auto *Child : Children)
    Child->Cleanup();
  delete this;
}

void UnaryOperatorNode::Cleanup() {
  Child->Cleanup();
  delete this;
}

void MemberNode::Cleanup() {
  Base->Cleanup();
  delete this;
}

void ImplicitCastNode::Cleanup() {
  Child->Cleanup();
  delete this;
}
