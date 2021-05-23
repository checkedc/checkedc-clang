// Tests for updating the observed bounds context during bounds inference and checking.
// This file tests updating the context mapping member expressions to their bounds
// after checking expressions during bounds analysis.
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state -verify %s | FileCheck %s

#include <stdchecked.h>

// expected-no-diagnostics

struct S {
  int len;
  array_ptr<int> p : count(len);
  int i;
  array_ptr<int> q : count(i);
  array_ptr<int> r : count(i);
  array_ptr<int> f : count(3);
  array_ptr<int> g : bounds(f, f + 3);
};

// Assignment to a struct member that kills the bounds of another struct member
void kill_bounds1(struct S *s) {
  // Observed bounds context after assignment: { s->p => bounds(unknown) }
  s->len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->p
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }

  // Observed bounds context after assignment: { s->p => bounds(unknown) }
  (*s).len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->p
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }

  // Observed bounds context after assignment: { s->p => bounds(unknown) }
  s[0].len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->p
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}

// Assignment to a struct member that kills the bounds of multiple struct members
void kill_bounds2(struct S *s) {
  // Observed bounds context after assignment: { s[3].q => bounds(unknown), s[3].r => bounds(unknown) }
  s[3].i = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} .q
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} .r
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }

  // Observed bounds context after assignment: { s[3].q => bounds(unknown), s[3].r => bounds(unknown) }
  (1 + 2)[s].i = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} .q
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} .r
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}

// Incrementing a struct member used in its own bounds
void kill_bounds3(struct S *s) {
  // The assignment s->p++ is not invertible with respect to s->p
  // (we do not calculate an inverse for member expressions),
  // so s->p has no original value.
  // Observed bounds context after assignment: { s->p => bounds(unknown) }
  s->p++;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} '++'
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->p
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}

// Set the inferred bounds of the LHS to the inferred bounds of the RHS
void updated_source_bounds1(struct S *s) {
  // Observed bounds context after assignment: { s->p => bounds(any) }
  s->p = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->p
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
  // CHECK-NEXT: }
}

// Set the inferred bounds of the LHS to the inferred bounds of the RHS
void updated_source_bounds2(struct S *s) {
  // Observed bounds context after assignment: { s[4].q => bounds(s[4].r, s[4].r + s[4].i) }
  s[4].q = s[4].r;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} .q
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     MemberExpr {{.*}} .r
  // CHECK-NEXT:       ArraySubscriptExpr
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 4
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} .r
  // CHECK-NEXT:         ArraySubscriptExpr
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 4
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} .i
  // CHECK-NEXT:         ArraySubscriptExpr
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 4
}

// Assigning to a struct member used in the bounds of the RHS
void updated_source_bounds3(struct S *s) {
  // Observed bounds context after assignment: { s->f => bounds(unknown), s->g => bounds(unknown) }
  // Note: we do not currently compute an original value for s->f.
  // Original source bounds for the RHS: bounds(s->f, s->f + 3)
  // Adjusted source bounds returned for the RHS: bounds(s->f, s->f + 3)
  s->f = s->f + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->f
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->g
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }

  // Observed bounds context after assignment: { s->f => bounds(unknown), s->g => bounds(unknown) }
  // Note: we do not currently compute an original value for s->f.
  // Original source bounds for the RHS: bounds(s->f, s->f + 3)
  // Adjusted source bounds returned for the RHS: bounds(s->f, s->f + 3)
  s->f = s->g;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->f
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->g
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}

struct C {
  int len;
  array_ptr<int> r : count(len);
};

struct B {
  struct C c;
  array_ptr<int> q : count(c.len);
};

struct A {
  struct B b;
  array_ptr<int> p : count(b.c.len);
};

// Assigning to a nested member expression
void nested1(struct A a) {
  // Observed bounds context after assignment: { a.p => bounds(unknown), a.b.q => bounds(unknown), a.b.c.r => bounds(unknown) }
  a.b.c.len = 0;
  // CHECK: Statement S:
  // CHECK: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} .p
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} .q
  // CHECK-NEXT:   MemberExpr {{.*}} .b
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} .r
  // CHECK-NEXT:   MemberExpr {{.*}} .c
  // CHECK-NEXT:     MemberExpr {{.*}} .b
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}
