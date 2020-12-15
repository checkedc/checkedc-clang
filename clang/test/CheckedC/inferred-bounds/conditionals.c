// Tests of inferred bounds for expressions involving conditional operators.
// The goal is to check that the bounds are being inferred correctly.
//
// The tests have the general form:
// 1. Some C code.
// 2. A description of the inferred bounds for that C code:
//  a. The expression
//  b. The inferred bounds.
// The description uses AST dumps.
//
// This line is for the clang test infrastructure:
// RUN: %clang_cc1 -fcheckedc-extension -verify -fdump-inferred-bounds %s | FileCheck %s

#include <stdchecked.h>

// Bounds in each arm are equivalent
void f1(array_ptr<int> q : count(2), array_ptr<int> r : count(2)) {
  // Ensure q and r produce the same value so that bounds(q, q + 2) and bounds(r, r + 2) are equivalent
  q = 0, r = 0;

  // Declared LHS bounds: bounds(p, p + 1)
  // Initializer RHS bounds: bounds(q, q + 2)
  // Conditional true arm bounds: bounds(q, q + 2)
  // Conditional false arm bounds: bounds(r, r + 2)
  array_ptr<int> p : count(1) = 1 ? q : r;
  // CHECK: VarDecl {{.*}} p
  // CHECK:   CountBoundsExpr {{.*}} Element
  // CHECK:     IntegerLiteral {{.*}} 1
  // CHECK:   ConditionalOperator
  // CHECK:     IntegerLiteral {{.*}} 1
  // CHECK:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'q'
  // CHECK:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'r'
  // CHECK: Declared Bounds:
  // CHECK: CountBoundsExpr {{.*}} Element
  // CHECK:   IntegerLiteral {{.*}} 1
  // CHECK: Initializer Bounds:
  // CHECK: RangeBoundsExpr
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:     DeclRefExpr {{.*}} 'q'
  // CHECK:   BinaryOperator {{.*}} '+'
  // CHECK:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'q'
  // CHECK:     IntegerLiteral {{.*}} 2
}

// Bounds in each arm are equivalent
void f2(array_ptr<int> b : count(3)) {
  // Declared LHS bounds: bounds(a, a + 3)
  // Initializer RHS bounds: bounds(b, b + 3)
  // Conditional true arm bounds: bounds(b, b + 3)
  // Conditional false arm bounds: bounds(b, b + 3)
  // a and b are not known to be equivalent after this statement.
  array_ptr<int> a : count(3) = 1 ? b : b + 2; // expected-warning {{cannot prove declared bounds for 'a' are valid after initialization}} \
                                               // expected-note {{(expanded) declared bounds are 'bounds(a, a + 3)'}} \
                                               // expected-note {{(expanded) inferred bounds are 'bounds(b, b + 3)'}}
  // CHECK: VarDecl {{.*}} a
  // CHECK:   CountBoundsExpr {{.*}} Element
  // CHECK:     IntegerLiteral {{.*}} 3
  // CHECK:   ConditionalOperator
  // CHECK:     IntegerLiteral {{.*}} 1
  // CHECK:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'b'
  // CHECK:     BinaryOperator {{.*}} '+'
  // CHECK:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:         DeclRefExpr {{.*}} 'b'
  // CHECK:       IntegerLiteral {{.*}} 2
  // CHECK: Declared Bounds:
  // CHECK: CountBoundsExpr {{.*}} Element
  // CHECK:   IntegerLiteral {{.*}} 3
  // CHECK: Initializer Bounds:
  // CHECK: RangeBoundsExpr
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:     DeclRefExpr {{.*}} 'b'
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'b'
  // CHECK:     IntegerLiteral {{.*}} 3
}

// Bounds in one arm are bounds(any)
void f3(array_ptr<int> p : count(1), array_ptr<int> q : count(1)) {
  // Ensure that p and q produce the same value so that bounds(q, q + 1) imply bounds(p, p + 1).
  p = 0, q = 0;

  // Target LHS bounds: bounds(p, p + 1)
  // Inferred RHS bounds: bounds(q, q + 1)
  // Conditional true arm bounds: bounds(q, q + 1)
  // Conditional false arm bounds: bounds(any)
  p = (1 ? q : 0);
  // CHECK: BinaryOperator {{.*}} '='
  // CHECK:   DeclRefExpr {{.*}} 'p'
  // CHECK:   ParenExpr
  // CHECK:     ConditionalOperator
  // CHECK:       IntegerLiteral {{.*}} 1
  // CHECK:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:         DeclRefExpr {{.*}} 'q'
  // CHECK:       ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK:         IntegerLiteral {{.*}} 0
  // CHECK: Target Bounds:
  // CHECK: RangeBoundsExpr
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:     DeclRefExpr {{.*}} 'p'
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'p'
  // CHECK:     IntegerLiteral {{.*}} 1
  // CHECK: RHS Bounds:
  // CHECK: RangeBoundsExpr
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:     DeclRefExpr {{.*}} 'q'
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'q'
  // CHECK:     IntegerLiteral {{.*}} 1

  // Target LHS bounds: bounds(p, p + 1)
  // Inferred RHS bounds: bounds(q, q + 1)
  // Conditional true arm bounds: bounds(any)
  // Conditional false arm bounds: bounds(q, q + 1)
  p = (1 ? 0 : q);
  // CHECK: BinaryOperator {{.*}} '='
  // CHECK:   DeclRefExpr {{.*}} 'p'
  // CHECK:   ParenExpr
  // CHECK:     ConditionalOperator
  // CHECK:       IntegerLiteral {{.*}} 1
  // CHECK:       ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK:         IntegerLiteral {{.*}} 0
  // CHECK:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:         DeclRefExpr {{.*}} 'q'
  // CHECK: Target Bounds:
  // CHECK: RangeBoundsExpr
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:     DeclRefExpr {{.*}} 'p'
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'p'
  // CHECK:     IntegerLiteral {{.*}} 1
  // CHECK: RHS Bounds:
  // CHECK: RangeBoundsExpr
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:     DeclRefExpr {{.*}} 'q'
  // CHECK:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'q'
  // CHECK:     IntegerLiteral {{.*}} 1
}

// Bounds in conditional arms are not equal (and neither is bounds(any))
void f4(array_ptr<int> b : count(5), array_ptr<int> c : count(6)) {
  // Declared LHS bounds: bounds(a, a + 4)
  // Initializer RHS bounds: bounds(unknown)
  // Conditional true arm bounds: bounds(b, b + 5)
  // Conditional false arm bounds: bounds(c, c + 6)
  array_ptr<int> a : count(4) = 1 ? b : c; // expected-error {{inferred bounds for 'a' are unknown after initialization}} \
                                           // expected-note {{(expanded) declared bounds are 'bounds(a, a + 4)'}}
  // CHECK: VarDecl {{.*}} a
  // CHECK:   CountBoundsExpr {{.*}} Element
  // CHECK:     IntegerLiteral {{.*}} 4
  // CHECK:   ConditionalOperator
  // CHECK:     IntegerLiteral {{.*}} 1
  // CHECK:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'b'
  // CHECK:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK:       DeclRefExpr {{.*}} 'c'
  // CHECK: Declared Bounds:
  // CHECK: CountBoundsExpr {{.*}} Element
  // CHECK:   IntegerLiteral {{.*}} 4
  // CHECK: Initializer Bounds:
  // CHECK: NullaryBoundsExpr {{.*}} Invalid
}