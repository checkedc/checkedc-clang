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

void PreorderAST::AddNode(Node *N, Node *Parent) {
  // If the root is null, make the current node the root.
  if (!Root)
    Root = N;

  // Add the current node to the list of children of its parent.
  if (Parent) {
    assert(isa<BinaryNode>(Parent) && "Invalid parent");
    dyn_cast<BinaryNode>(Parent)->Children.push_back(N);
  }
}

void PreorderAST::RemoveNode(Node *N, Node *Parent) {
  // The parent should be a BinaryNode.
  assert(isa<BinaryNode>(Parent) && "Invalid parent");

  auto *P = dyn_cast<BinaryNode>(Parent);
  if (!P) {
    SetError();
    return;
  }

  // We will remove a BinaryNode only if its operator is equal to its
  // parent's operator and the operator is commutative and associative.
  if (auto *B = dyn_cast<BinaryNode>(N)) {
    assert(B->Opc == P->Opc &&
           "BinaryNode operator must equal parent operator");

    assert(B->IsOpCommutativeAndAssociative() &&
           "BinaryNode operator must be commutative and associative");

    if (B->Opc != P->Opc ||
       !B->IsOpCommutativeAndAssociative()) {
      SetError();
      return;
    }
  }

  // Remove the current node from the list of children of its parent.
  for (auto I = P->Children.begin(),
            E = P->Children.end(); I != E; ++I) {
    if (*I == N) {
      P->Children.erase(I);
      break;
    }
  }

  if (auto *B = dyn_cast<BinaryNode>(N)) {
    // Move all children of the current node to its parent.
    for (auto *Child : B->Children) {
      Child->Parent = P;
      P->Children.push_back(Child);
    }
  }

  // Delete the current node.
  delete N;
}

void PreorderAST::Create(Expr *E, Node *Parent) {
  if (!E)
    return;

  E = Lex.IgnoreValuePreservingOperations(Ctx, E->IgnoreParens());

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    BinaryOperator::Opcode BinOp = BO->getOpcode();
    Expr *LHS = BO->getLHS();
    Expr *RHS = BO->getRHS();

    // We can convert (e1 - e2) to (e1 + -e2) if -e2 does not overflow.  One
    // instance where -e2 can overflow is if e2 is INT_MIN. Here, instead of
    // specifically checking whether e2 is INT_MIN, we add a unary minus to e2
    // and then check if the resultant expression -e2 overflows. If it
    // overflows, we undo the unary minus operator.

    // TODO: Currently, we can only prove that integer constant expressions do
    // not overflow. We still need to handle proving that non-constant
    // expressions do not overflow.
    if (BO->getOpcode() == BO_Sub &&
        RHS->isIntegerConstantExpr(Ctx)) {
      Expr *UOMinusRHS = new (Ctx) UnaryOperator(RHS, UO_Minus, RHS->getType(),
                                             RHS->getValueKind(),
                                             RHS->getObjectKind(),
                                             SourceLocation(),
                                             /*CanOverflow*/ true);
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

    auto *N = new BinaryNode(BinOp, Parent);
    AddNode(N, Parent);

    Create(LHS, /*Parent*/ N);
    Create(RHS, /*Parent*/ N);

  } else if (!Parent) {
    // The invariant is that the root node must be a BinaryNode. So for
    // expressions like "if (*p)", we don't have a BinaryOperator. So when we
    // enter this function there is no root and the parent is null. So we
    // create a new BinaryNode with + as the operator and add "p" as a
    // LeafNodeExpr child of this BinaryNode. Later, in the function
    // NormalizeExprsWithoutConst we normalize "p" to "p + 0" by adding 0 as a
    // sibling of "p".

    auto *N = new BinaryNode(BO_Add, Parent);
    AddNode(N, Parent);
    Create(E, /*Parent*/ N);

  } else {
    auto *N = new LeafExprNode(E, Parent);
    AddNode(N, Parent);
  }
}

void PreorderAST::Coalesce(Node *N, bool &Changed) {
  if (Error)
    return;

  auto *B = dyn_cast_or_null<BinaryNode>(N);
  if (!B)
    return;

  // Coalesce the children first.
  for (auto *Child : B->Children)
    if (isa<BinaryNode>(Child))
      Coalesce(Child, Changed);

  // We can only coalesce if the operator is commutative and associative.
  if (!B->IsOpCommutativeAndAssociative())
    return;

  auto *Parent = dyn_cast_or_null<BinaryNode>(B->Parent);
  if (!Parent)
    return;

  // We can coalesce only if:
  // 1. The parent has the same operator as the current node.
  // 2. The current node is a BinaryNode with just one child (for example, as a
  // result of constant folding).
  if (Parent->Opc == B->Opc || B->Children.size() == 1) {
    RemoveNode(B, Parent);
    Changed = true;
  }
}

bool PreorderAST::CompareNodes(const Node *N1, const Node *N2) {
  if (const auto *L1 = dyn_cast<LeafExprNode>(N1)) {
    if (const auto *L2 = dyn_cast<LeafExprNode>(N2)) {
      // If L1 is a UnaryOperatorExpr and L2 is not, then
      // 1. If L1 contains an integer constant then sorted order is (L2, L1)
      // 2. Else sorted order is (L1, L2).
      if (isa<UnaryOperator>(L1->E) && !isa<UnaryOperator>(L2->E))
        return !L1->E->isIntegerConstantExpr(Ctx);

      // If L2 is a UnaryOperatorExpr and L1 is not, then
      // 1. If L2 contains an integer constant then sorted order is (L1, L2)
      // 2. Else sorted order is (L2, L1).
      if (!isa<UnaryOperator>(L1->E) && isa<UnaryOperator>(L2->E))
        return L2->E->isIntegerConstantExpr(Ctx);

      // If both nodes are LeafExprNodes compare the exprs.
      return Lex.CompareExpr(L1->E, L2->E) == Result::LessThan;
    }

    // N2:BinaryNodeExpr < N1:LeafExprNode.
    return false;
  }

  // N1:BinaryNodeExpr < N2:LeafExprNode.
  if (isa<LeafExprNode>(N2))
    return true;

  // Compare N1:BinaryNode and N2:BinaryNode.
  const auto *B1 = dyn_cast<BinaryNode>(N1);
  const auto *B2 = dyn_cast<BinaryNode>(N2);

  if (B1->Opc != B2->Opc)
    return B1->Opc < B2->Opc;
  return B1->Children.size() < B2->Children.size();
}

void PreorderAST::Sort(Node *N) {
  auto *B = dyn_cast_or_null<BinaryNode>(N);
  if (!B)
    return;

  // Sort the children first.
  for (auto *Child : B->Children)
    if (isa<BinaryNode>(Child))
      Sort(Child);

  // We can only sort if the operator is commutative and associative.
  if (!B->IsOpCommutativeAndAssociative())
    return;

  // Sort the children.
  llvm::sort(B->Children.begin(), B->Children.end(),
             [&](const Node *N1, const Node *N2) {
               return CompareNodes(N1, N2);
            });
}

void PreorderAST::ConstantFold(Node *N, bool &Changed) {
  // Note: This function assumes that the children of each BinaryNode of the
  // preorder AST have already been sorted.

  if (Error)
    return;

  auto *B = dyn_cast_or_null<BinaryNode>(N);
  if (!B)
    return;

  size_t ConstStartIdx = 0;
  unsigned NumConsts = 0;
  llvm::APSInt ConstFoldedVal;

  for (size_t I = 0; I != B->Children.size(); ++I) {
    auto *Child = B->Children[I];

    // Recursively constant fold the children of a BinaryNode.
    if (isa<BinaryNode>(Child)) {
      ConstantFold(Child, Changed);
      continue;
    }

    // We can only constant fold if the operator is commutative and
    // associative.
    if (!B->IsOpCommutativeAndAssociative())
      continue;

    auto *ChildLeafNode = dyn_cast_or_null<LeafExprNode>(Child);
    if (!ChildLeafNode)
      continue;

    // Check if the child node is an integer constant.
    llvm::APSInt CurrConstVal;
    if (!ChildLeafNode->E->isIntegerConstantExpr(CurrConstVal, Ctx))
      continue;

    ++NumConsts;

    if (NumConsts == 1) {
      // We will use ConstStartIdx later in this function to delete the
      // constant folded nodes.
      ConstStartIdx = I;
      ConstFoldedVal = CurrConstVal;

    } else {
      // Constant fold based on the operator.
      bool Overflow;
      switch(B->Opc) {
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
        SetError();
        return;
      }
    }
  }

  // To fold constants we need at least 2 constants.
  if (NumConsts <= 1)
    return;

  // Delete the folded constants and reclaim memory.
  // Note: We do not explicitly need to increment the iterator because after
  // erase the iterator automatically points to the new location of the element
  // following the one we just erased.
  llvm::SmallVector<Node *, 2>::iterator I =
    B->Children.begin() + ConstStartIdx;
  while (NumConsts--) {
    delete(*I);
    B->Children.erase(I);
  }

  llvm::APInt IntVal(Ctx.getTargetInfo().getIntWidth(),
                     ConstFoldedVal.getLimitedValue());

  Expr *ConstFoldedExpr = new (Ctx) IntegerLiteral(Ctx, IntVal, Ctx.IntTy,
                                                   SourceLocation());

  // Add the constant folded expression to list of children of the current
  // BinaryNode.
  B->Children.push_back(new LeafExprNode(ConstFoldedExpr, B));
  Changed = true;
}

void PreorderAST::NormalizeExprsWithoutConst(Node *N) {
  // Consider the following case:
  // Upper bound expr: p
  // Deref expr: p + 1
  // In this case, we would not able able to extract the offset from the deref
  // expression because the upper bound expression does not contain a constant.
  // This is because the node-by-node comparison of the two expressions would
  // fail. So we require that expressions be of the form "(variable + constant)".
  // So, we normalize expressions by adding an integer constant to expressions
  // which do not have one. For example:
  // p ==> (p + 0)
  // (p + i) ==> (p + i + 0)
  // (p * i) ==> (p * i * 1)

  auto *B = dyn_cast_or_null<BinaryNode>(N);
  if (!B)
    return;

  for (auto *Child : B->Children) {
    // Recursively normalize constants in the children of a BinaryNode.
    if (isa<BinaryNode>(Child))
      NormalizeExprsWithoutConst(Child);

    else if (auto *ChildLeafNode = dyn_cast<LeafExprNode>(Child)) {
      if (ChildLeafNode->E->isIntegerConstantExpr(Ctx))
        return;
    }
  }

  llvm::APInt IntConst;
  switch(B->Opc) {
    default: return;
    case BO_Add:
      IntConst = llvm::APInt(Ctx.getTargetInfo().getIntWidth(), 0);
      break;
    case BO_Mul:
      IntConst = llvm::APInt(Ctx.getTargetInfo().getIntWidth(), 1);
      break;
  }

  auto *IntLiteral = new (Ctx) IntegerLiteral(Ctx, IntConst, Ctx.IntTy,
                                              SourceLocation());
  auto *L = new LeafExprNode(IntLiteral, B);
  AddNode(L, B);
}

bool PreorderAST::IsEqual(Node *N1, Node *N2) {
  // If both the nodes are null.
  if (!N1 && !N2)
    return true;

  // If only one of the nodes is null.
  if ((N1 && !N2) || (!N1 && N2))
    return false;

  if (const auto *B1 = dyn_cast<BinaryNode>(N1)) {
    // If the types of the nodes mismatch.
    if (!isa<BinaryNode>(N2))
      return false;

    const auto *B2 = dyn_cast<BinaryNode>(N2);

    // If the Opcodes mismatch.
    if (B1->Opc != B2->Opc)
      return false;

    // If the number of children of the two nodes mismatch.
    if (B1->Children.size() != B2->Children.size())
      return false;

    // Match each child of the two nodes.
    for (size_t I = 0; I != B1->Children.size(); ++I) {
      auto *Child1 = B1->Children[I];
      auto *Child2 = B2->Children[I];

      // If any child differs between the two nodes.
      if (!IsEqual(Child1, Child2))
        return false;
    }
  }

  if (const auto *L1 = dyn_cast<LeafExprNode>(N1)) {
    // If the expr differs between the two nodes.
    if (const auto *L2 = dyn_cast<LeafExprNode>(N2))
      return Lex.CompareExpr(L1->E, L2->E) == Result::Equal;

    // Else if the types of the nodes mismatch.
    return false;
  }

  return true;
}

void PreorderAST::Normalize() {
  // TODO: Perform simple arithmetic optimizations/transformations on the
  // constants in the nodes.

  bool Changed = true;
  while (Changed) {
    Changed = false;
    Coalesce(Root, Changed);
    if (Error)
      break;
    Sort(Root);
    ConstantFold(Root, Changed);
    if (Error)
      break;
    NormalizeExprsWithoutConst(Root);
  }

  if (Ctx.getLangOpts().DumpPreorderAST) {
    PrettyPrint(Root);
    OS << "--------------------------------------\n";
  }
}

void PreorderAST::PrettyPrint(Node *N) {
  if (const auto *B = dyn_cast_or_null<BinaryNode>(N)) {
    OS << BinaryOperator::getOpcodeStr(B->Opc) << "\n";

    for (auto *Child : B->Children)
      PrettyPrint(Child);
  }
  else if (const auto *L = dyn_cast_or_null<LeafExprNode>(N))
    L->E->dump(OS);
}

void PreorderAST::Cleanup(Node *N) {
  if (auto *B = dyn_cast_or_null<BinaryNode>(N))
    for (auto *Child : B->Children)
      Cleanup(Child);

  if (N)
    delete N;
}
