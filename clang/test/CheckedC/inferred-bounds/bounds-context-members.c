// Tests for updating the observed bounds context during bounds inference and checking.
// This file tests updating the context mapping member expressions to their bounds
// after checking expressions during bounds analysis.
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state -verify %s | FileCheck %s

#include <stdchecked.h>

struct S {
  int len;
  array_ptr<int> p : count(len); // expected-note 5 {{(expanded) declared bounds are 'bounds(s->p, s->p + s->len)'}}
  int i;
  array_ptr<int> q : count(i); // expected-note 2 {{(expanded) declared bounds are 'bounds(s[3].q, s[3].q + s[3].i)'}} \
                               // expected-note {{(expanded) declared bounds are 'bounds(s[4].q, s[4].q + s[4].i)'}}
  array_ptr<int> r : count(i); // expected-note 2 {{(expanded) declared bounds are 'bounds(s[3].r, s[3].r + s[3].i)'}}
  array_ptr<int> f : count(3); // expected-note 2 {{(expanded) declared bounds are 'bounds(s->f, s->f + 3)'}}
  array_ptr<int> g : bounds(f, f + 3); // expected-note 2 {{(expanded) declared bounds are 'bounds(s->f, s->f + 3)'}}
  array_ptr<int> a : count(2); // expected-note {{(expanded) declared bounds are 'bounds(s->a, s->a + 2)'}}
  array_ptr<int> b : count(2);
};

// Assignment to a struct member that kills the bounds of another struct member
void kill_bounds1(struct S *s) {
  // Observed bounds context after assignment: { s->p => bounds(unknown) }
  s->len = 0; // expected-error {{inferred bounds for 's->p' are unknown after assignment}} \
              // expected-note {{lost the value of the expression 's->len' which is used in the (expanded) inferred bounds 'bounds(s->p, s->p + s->len)' of 's->p'}}
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
  (*s).len = 0; // expected-error {{inferred bounds for 's->p' are unknown after assignment}} \
                // expected-note {{lost the value of the expression '(*s).len' which is used in the (expanded) inferred bounds 'bounds(s->p, s->p + s->len)' of 's->p'}}
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
  s[0].len = 0; // expected-error {{inferred bounds for 's->p' are unknown after assignment}} \
                // expected-note {{lost the value of the expression 's[0].len' which is used in the (expanded) inferred bounds 'bounds(s->p, s->p + s->len)' of 's->p'}}
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
  s[3].i = 0; // expected-error {{inferred bounds for 's[3].q' are unknown after assignment}} \
              // expected-note {{lost the value of the expression 's[3].i' which is used in the (expanded) inferred bounds 'bounds(s[3].q, s[3].q + s[3].i)' of 's[3].q'}} \
              // expected-error {{inferred bounds for 's[3].r' are unknown after assignment}} \
              // expected-note {{lost the value of the expression 's[3].i' which is used in the (expanded) inferred bounds 'bounds(s[3].r, s[3].r + s[3].i)' of 's[3].r'}}
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
  (1 + 2)[s].i = 0; // expected-error {{inferred bounds for 's[3].q' are unknown after assignment}} \
                    // expected-note {{lost the value of the expression '(1 + 2)[s].i' which is used in the (expanded) inferred bounds 'bounds(s[3].q, s[3].q + s[3].i)' of 's[3].q'}} \
                    // expected-error {{inferred bounds for 's[3].r' are unknown after assignment}} \
                    // expected-note {{lost the value of the expression '(1 + 2)[s].i' which is used in the (expanded) inferred bounds 'bounds(s[3].r, s[3].r + s[3].i)' of 's[3].r'}} \
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
  // Original value of s->p after increment: s->p - 1
  // Observed bounds context after increment: { s->p => bounds(s->p - 1, s->p - 1 + s->len) }
  s->p++; // expected-warning {{cannot prove declared bounds for 's->p' are valid after increment}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(s->p - 1, s->p - 1 + s->len)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} '++'
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->p
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} ->p
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         MemberExpr {{.*}} ->p
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} ->len
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 's'
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

  // Observed bounds context after assignment: { s[4].q => bounds(s[3].r, s[3].r + s[3].i) }
  // s[4].q and s[3].r are (temporarily) equivalent, but s[4].i and s[3].i
  // are not equivalent, so we get a warning.
  s[4].q = s[3].r; // expected-warning {{cannot prove declared bounds for 's[4].q' are valid after assignment}} \
                   // expected-note {{(expanded) inferred bounds are 'bounds(s[3].r, s[3].r + s[3].i)'}}
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
  // CHECK-NEXT:         IntegerLiteral {{.*}} 3
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} .r
  // CHECK-NEXT:         ArraySubscriptExpr
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 3
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} .i
  // CHECK-NEXT:         ArraySubscriptExpr
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 3

  // Observed bounds after assignment: { s[7].a => bounds(s[8].b, s[8].b + 2) }
  // s[7].a and s[8].b are (temporarily) equivalent, so we get no warning.
  s[7].a = s[8].b;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} .a
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 7
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     MemberExpr {{.*}} .b
  // CHECK-NEXT:       ArraySubscriptExpr
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 8
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} .b
  // CHECK-NEXT:         ArraySubscriptExpr
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 8
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
}

// Assigning to a struct member used in the bounds of the RHS
void updated_source_bounds3(struct S *s) {
  // Original value of s->f: s->f - 1
  // Observed bounds context after assignment: { s->f => bounds(s->f + 2, s->f + 2 + 3), s->g => bounds(s->f + 2, s->f + 2 + 3) }
  s->f = s->f - 2; // expected-warning {{cannot prove declared bounds for 's->f' are valid after assignment}} \
                   // expected-note {{(expanded) inferred bounds are 'bounds(s->f + 2, s->f + 2 + 3)'}} \
                   // expected-warning {{cannot prove declared bounds for 's->g' are valid after assignment}} \
                   // expected-note {{(expanded) inferred bounds are 'bounds(s->f + 2, s->f + 2 + 3)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->f
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} ->f
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         MemberExpr {{.*}} ->f
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->g
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} ->f
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         MemberExpr {{.*}} ->f
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }

  // Observed bounds context after assignment: { s->f => bounds(unknown), s->g => bounds(unknown) }
  // This is not an invertible assignment, so s->f has no original value.
  s->f = s->g; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
               // expected-note {{lost the value of the expression 's->f' which is used in the (expanded) inferred bounds 'bounds(s->f, s->f + 3)' of 's->f'}} \
               // expected-error {{inferred bounds for 's->g' are unknown after assignment}} \
               // expected-note {{lost the value of the expression 's->f' which is used in the (expanded) inferred bounds 'bounds(s->f, s->f + 3)' of 's->g'}}
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

void multiple_assignments1(struct S *s, _Array_ptr<int> arr : count(3)) {
  // Observed bounds context after statement: { s->p => bounds(arr, arr + 3), arr => bounds(arr, arr + 3) }
  s->p = arr, s->len = 3;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->p
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }

  // Observed bounds context after assignment: { s->p => bounds(unknown), arr => bounds(arr, arr + 3) }
  s[0].len = 0; // expected-error {{inferred bounds for 's->p' are unknown after assignment}} \
                // expected-note {{lost the value of the expression 's[0].len' which is used in the (expanded) inferred bounds 'bounds(s->p, s->p + s->len)' of 's->p'}}
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
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }
}

void multiple_assignments2(struct S *s) {
  // Observed bounds context after statement: { s->a => bounds(s->b, s->b + 2) }
  s->a = s->b, s->a++;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->a
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     MemberExpr {{.*}} ->b
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} ->b
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }

  // Observed bounds context after statement: { s->a => bounds(unknown), s->b = bounds(any) }
  s->a = s->b, s->b = 0; // expected-error {{inferred bounds for 's->a' are unknown after assignment}} \
                         // expected-note {{lost the value of the expression 's->b' which is used in the (expanded) inferred bounds 'bounds(s->b, s->b + 2)' of 's->a'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->a
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->b
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
  // CHECK-NEXT: }
}

void multiple_assignments3(struct S *s, int i) {
  // Observed bounds context after statement: { s->a = bounds(s->f, s->f + 3) }
  // The access s->a[2] is within s->a's observed bounds of bounds(s->f, s->f + 3)
  s->a = s->f, i = s->a[2];
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: MemberExpr {{.*}} ->a
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     MemberExpr {{.*}} ->f
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       MemberExpr {{.*}} ->f
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }
}

struct C {
  int len;
  array_ptr<int> r : count(len); // expected-note {{(expanded) declared bounds are 'bounds(a.b.c.r, a.b.c.r + a.b.c.len)'}}
};

struct B {
  struct C c;
  array_ptr<int> q : count(c.len); // expected-note {{(expanded) declared bounds are 'bounds(a.b.q, a.b.q + a.b.c.len)'}}
};

struct A {
  struct B b;
  array_ptr<int> p : count(b.c.len); // expected-note {{(expanded) declared bounds are 'bounds(a.p, a.p + a.b.c.len)'}}
};

// Assigning to a nested member expression
void nested1(struct A a) {
  // Observed bounds context after assignment: { a.p => bounds(unknown), a.b.q => bounds(unknown), a.b.c.r => bounds(unknown) }
  a.b.c.len = 0; // expected-error {{inferred bounds for 'a.p' are unknown after assignment}} \
                 // expected-note {{lost the value of the expression 'a.b.c.len' which is used in the (expanded) inferred bounds 'bounds(a.p, a.p + a.b.c.len)' of 'a.p'}} \
                 // expected-error {{inferred bounds for 'a.b.q' are unknown after assignment}} \
                 // expected-note {{lost the value of the expression 'a.b.c.len' which is used in the (expanded) inferred bounds 'bounds(a.b.q, a.b.q + a.b.c.len)' of 'a.b.q'}} \
                 // expected-error {{inferred bounds for 'a.b.c.r' are unknown after assignment}} \
                 // expected-note {{lost the value of the expression 'a.b.c.len' which is used in the (expanded) inferred bounds 'bounds(a.b.c.r, a.b.c.r + a.b.c.len)' of 'a.b.c.r'}}
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
