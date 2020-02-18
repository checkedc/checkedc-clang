// Tests for updating equivalent expression information during bounds inference and checking.
// This file tests updating the set of expressions that produces the same value as an expression
// after checking the expression during bounds analysis.
// This file does not test assignments that update the set of sets of equivalent expressions
// (assignments will be tested in a separate test file).
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s --dump-input=always

#include <stdchecked.h>

extern int a1 [12];
extern void g1(void);
extern void g2(int i);
extern void g3(array_ptr<int> arr : count(1));

// DeclRefExpr
void f1(int i, int a checked[5]) {
  // Non-array type
  i;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Local checked array with known size
  int arr checked[10];
  arr;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }

  // Extern unchecked array with known size
  a1;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'a1'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a1'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a1'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a1'
  // CHECK-NEXT: }

  // Array parameter with _Array_ptr<int> type
  a;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
}

// UnaryOperator: non-increment/decrement operators
void f2(int *p, int x, int y) {
  *p;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} '*'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   UnaryOperator {{.*}} '*'
  // CHECK-NEXT:    ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:      DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }

  &x;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }

  int a[3];
  &a;
  // CHECK: Statement S:
  // CHECK: DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }

  !y;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} '!'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '!'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
}

// ArraySubscriptExpr
void f3(int a [1]) {
  a[0];
  // CHECK: Statement S:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ArraySubscriptExpr {{.*}} 'int'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   ArraySubscriptExpr {{.*}} lvalue
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}

// IntegerLiteral, StringLiteral, CHKCBindTemporaryExpr
void f4() {
  5;
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 5
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 5
  // CHECK-NEXT: }

  "abc";
  // CHECK: Statement S:
  // CHECK-NEXT: StringLiteral {{.*}} "abc"
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: StringLiteral {{.*}} "abc"
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: CHKCBindTemporaryExpr {{.*}} 'char [4]'
  // CHECK-NEXT:   StringLiteral {{.*}} "abc"
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: CHKCBindTemporaryExpr {{.*}} 'char [4]'
  // CHECK-NEXT:   StringLiteral {{.*}} "abc"
  // CHECK-NEXT: BoundsValueExpr {{.*}} 'char [4]'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:   CHKCBindTemporaryExpr {{.*}} 'char [4]'
  // CHECK-NEXT:     StringLiteral {{.*}} "abc"
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: CHKCBindTemporaryExpr {{.*}} 'char [4]'
  // CHECK-NEXT:   StringLiteral {{.*}} "abc"
  // CHECK-NEXT: BoundsValueExpr {{.*}} 'char [4]'
  // CHECK-NEXT: }
}

// BinaryOperator: non-assignment, non-logical operators
void f5() {
  1 + 2;
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }

  3 < 4;
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 4
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '<'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 3
  // CHECK-NEXT:   IntegerLiteral {{.*}} 4
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '<'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 3
  // CHECK-NEXT:   IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }

  5 & 6;
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 5
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 5
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 6
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 6
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '&'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 5
  // CHECK-NEXT:   IntegerLiteral {{.*}} 6
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '&'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 5
  // CHECK-NEXT:   IntegerLiteral {{.*}} 6
  // CHECK-NEXT: }

  7, 8;
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 7
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 7
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 8
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 8
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   IntegerLiteral {{.*}} 7
  // CHECK-NEXT:   IntegerLiteral {{.*}} 8
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 8
  // CHECK-NEXT: }
}

// CStyleCastExpr, BoundsCastExpr
void f6(int i, array_ptr<int> arr : count(1)) {
  (double)i;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: CStyleCastExpr {{.*}} <IntegralToFloating>
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: CStyleCastExpr {{.*}} <IntegralToFloating>
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  _Assume_bounds_cast<array_ptr<char>>(arr, count(4));
  // CHECK: Statement S:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '&'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: BoundsCastExpr {{.*}} <AssumePtrBounds>
  // CHECK-NEXT:   CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:      DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   CountBoundsExpr
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BoundsCastExpr {{.*}} <AssumePtrBounds>
  // CHECK-NEXT:   CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   CountBoundsExpr
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: }
}

// CallExpr
void f7(void) {
  // Function with no parameters
  g1();
  // CHECK: Statement S:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'g1'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'g1'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'g1'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'g1'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: CallExpr {{.*}} 'void'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'g1'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }

  // Function with no parameter annotations
  g2(0);
  // CHECK: Statement S:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'g2'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'g2'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'g2'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'g2'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: CallExpr {{.*}} 'void'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'g2'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }

  // Function with parameter annotations
  g3(0);
  // CHECK: Statement S:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'g3'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: DeclRefExpr {{.*}} 'g3'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'g3'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'g3'
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: }
  // CHECK: Statement S:
  // CHECK-NEXT: CallExpr {{.*}} 'void'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'g3'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}
