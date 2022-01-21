//=--LowerBoundAssignment.h---------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Contains classes for detection and rewriting of assignment expression that
// would invalidate the bounds of pointers rewritten to use range bounds.
// For pointers fattend to use a fresh lower bound
// (`bounds(__3c_lower_bound_p, __3c_lower_bound_p + n)`), an
// assignment `p = q` effectively changes the lower bound of the range
// bounds, so that the new bounds of `p` are `bounds(q, q + n)`
// (assuming `q` has the same size as `p`). For this to not invalidate the
// bound, `__3c_lower_bound_p` must also be updated to be equal to `q`.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_LOWERBOUNDASSIGNMENT_H
#define LLVM_CLANG_3C_LOWERBOUNDASSIGNMENT_H

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/3C/ConstraintResolver.h"

// Return true if an assignment LHS=RHS is the value of RHS is not derived form
// LHS. For example, an assignment `p = q` will return true (we assume `q`
// doesn't alias `p`), while `p = p + 1` will return false.
bool isLowerBoundAssignment(clang::Expr *LHS, clang::Expr *RHS);

// A class to visit all lower bound assignment expression as detected by
// isLowerBoundAssignment. This class should be extended with
// visitLowerBoundAssignment overridden.
class LowerBoundAssignmentVisitor
  : public RecursiveASTVisitor<LowerBoundAssignmentVisitor> {
public:
  explicit LowerBoundAssignmentVisitor() {}

  bool VisitBinaryOperator(BinaryOperator *O);

  // Override this method to define the operation that should be performed on
  // each assignment. The LHS and RHS of the assignment expression are passed
  // through.
  virtual void visitLowerBoundAssignment(Expr *LHS, Expr *RHS) = 0;
};

// Visit each lower bound pointer expression and, if the LHS is a pointer
// variable that was rewritten to use range bounds, rewrite the assignment so
// that it doesn't not invalidate the bounds. e.g.:
//     q = p;
// becomes
//     __3c_lower_bound_q = p, q = __3c_lower_bound_q;
class LowerBoundAssignmentUpdater : public LowerBoundAssignmentVisitor {
public:
  explicit LowerBoundAssignmentUpdater(ASTContext *C, ProgramInfo &I,
                                       Rewriter &R) : ABInfo(
    I.getABoundsInfo()), CR(I, C), R(R) {}

  void visitLowerBoundAssignment(Expr *LHS, Expr *RHS) override;

private:
  AVarBoundsInfo &ABInfo;
  ConstraintResolver CR;
  Rewriter &R;
};

// Visit each lower bound assignment expression and, if it is inside a macro,
// mark the LHS pointer as ineligible for range bounds. This is required
// because, if the pointer is given range bounds, then the assignment expression
// would need to be rewritten. The expression is a macro, so it cannot be
// rewritten.
class LowerBoundAssignmentFinder : public LowerBoundAssignmentVisitor {
public:
  explicit LowerBoundAssignmentFinder(ASTContext *C, ProgramInfo &I) : ABInfo(
    I.getABoundsInfo()), CR(I, C), C(C) {}

  void visitLowerBoundAssignment(Expr *LHS, Expr *RHS) override;

private:
  AVarBoundsInfo &ABInfo;
  ConstraintResolver CR;
  ASTContext *C;
};
#endif // LLVM_CLANG_3C_LOWERBOUNDASSIGNMENT_H
