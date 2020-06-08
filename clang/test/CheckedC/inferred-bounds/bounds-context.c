// Tests for updating the observed bounds context during bounds inference and checking.
// This file tests updating the context mapping variables to their bounds
// after checking expressions during bounds analysis.
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s

#include <stdchecked.h>

extern void testNtArray(nt_array_ptr<char> p : count(0), int i);

// Parameter and local variables with declared count bounds
void f1(array_ptr<int> arr : count(len), int len, int size) {
  // Observed bounds context: { a => bounds(a, a + 5), arr => bounds(arr, arr + len) }
  array_ptr<int> a : count(5) = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} a
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       IntegerLiteral {{.*}} 5
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 5
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 5
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }

  // Observed bounds context: { a => bounds(a, a + 5), arr => bounds(arr, arr + len), b => bounds(b, b + size) }
  array_ptr<int> b : count(size) = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} b
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 5
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 5
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'size'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT: }
}

// If statement, redeclared variable
void f2(int flag, int x, int y) {
  // Observed bounds context: { a => bounds(a, a + x) }
  array_ptr<int> a : count(x) = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} a
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:         IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }

  if (flag) {
    // Observed bounds context: { a => bounds(a, a + x), a => bounds(a, a + y) }
    array_ptr<int> a : count(y) = 0;
    // CHECK: Statement S:
    // CHECK:      DeclStmt
    // CHECK-NEXT:   VarDecl {{.*}} a
    // CHECK-NEXT:     CountBoundsExpr
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <NullToPointer>
    // CHECK-NEXT:         IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: VarDecl {{.*}} a
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
    // CHECK: Variable:
    // CHECK-NEXT: VarDecl {{.*}} a
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: }

    // Observed bounds context: { a => bounds(a, a + x), a => bounds(a, a + y), b => bounds(b, b + y) }
    array_ptr<int> b : count(y) = 0;
    // CHECK: Statement S:
    // CHECK:      DeclStmt
    // CHECK-NEXT:   VarDecl {{.*}} b
    // CHECK-NEXT:     CountBoundsExpr
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <NullToPointer>
    // CHECK-NEXT:         IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: VarDecl {{.*}} a
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
    // CHECK: Variable:
    // CHECK-NEXT: VarDecl {{.*}} a
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
    // CHECK: Variable:
    // CHECK-NEXT: VarDecl {{.*}} b
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: }
  }

  // Observed bounds context: { a => bounds(a, a + x), c => bounds(c, c + x) }
  array_ptr<int> c : count(x) = a;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} c
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} c
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
}

// Widened bounds killed by a statement with multiple assignments
void f3(nt_array_ptr<int> p : count(i), int i, int other) {
  if (*(p + i)) {
    // Observed bounds context: { p => bounds(p, p + i) }
    // CHECK: Statement S:
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   UnaryOperator {{.*}} '*'
    // CHECK:          ParenExpr
    // CHECK-NEXT:       BinaryOperator {{.*}} '+'
    // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:           DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: ParmVarDecl {{.*}} p
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: }

    // Bounds of p are currently widened by 1
    // Observed bounds context: { p => bounds(p, (p + i) + 1) }
    p;
    // CHECK: Statement S:
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: ParmVarDecl {{.*}} p
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     BinaryOperator {{.*}} '+'
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT:     IntegerLiteral {{.*}} 1
    // CHECK-NEXT: }

    // This statement kills the widened bounds of p since it modifies i
    // Observed bounds context: { p => bounds(p, p + 1) }
    i++, --other;
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} ','
    // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT:   UnaryOperator {{.*}} prefix '--'
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'other'
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: ParmVarDecl {{.*}} p
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: }
  }
}

// Widened bounds killed by a statement with multiple assignments
void f4(nt_array_ptr<char> p : count(0), int other) {
  if (*p) {
    // Observed bounds context: { p => bounds(p, p + 0) }
    // CHECK: Statement S:
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   UnaryOperator {{.*}} '*'
    // CHECK:          ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK:      Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: ParmVarDecl {{.*}} p
    // CHECK-NEXT: CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:   IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     IntegerLiteral {{.*}} 0
    // CHECK-NEXT: }

    // Bounds of p are currently widened by 1
    // Observed bounds context: { p => bounds(p, (p + 0) + 1) }
    p[1];
    // CHECK: Statement S:
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   ArraySubscriptExpr
    // CHECK-NEXT:     Bounds Null-terminated read
    // CHECK-NEXT:       RangeBoundsExpr
    // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:         BinaryOperator {{.*}} '+'
    // CHECK-NEXT:           BinaryOperator {{.*}} '+'
    // CHECK-NEXT:             ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:               DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:             IntegerLiteral {{.*}} 0
    // CHECK-NEXT:           IntegerLiteral {{.*}} 1
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     IntegerLiteral {{.*}} 1
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: ParmVarDecl {{.*}} p
    // CHECK-NEXT: CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:   IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     BinaryOperator {{.*}} '+'
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:       IntegerLiteral {{.*}} 0
    // CHECK-NEXT:     IntegerLiteral {{.*}} 1
    // CHECK-NEXT: }

    // This statement kills the widened bounds of p since it modifies p
    // Observed bounds context: { p = bounds(p, p) }
    testNtArray(p = 0, other = 0);
    // CHECK: Statement S:
    // CHECK-NEXT: CallExpr {{.*}} 'void'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'testNtArray'
    // CHECK-NEXT:   BinaryOperator {{.*}} '='
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <NullToPointer>
    // CHECK-NEXT:       IntegerLiteral {{.*}} 0
    // CHECK-NEXT:   BinaryOperator {{.*}} '='
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'other'
    // CHECK-NEXT:     IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: ParmVarDecl {{.*}} p
    // CHECK-NEXT: CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:   IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     IntegerLiteral {{.*}} 0
    // CHECK-NEXT: }
  }
}

// Widened bounds of multiple variables killed by a statement with multiple assignments
void f5(nt_array_ptr<char> p : count(i), int i, nt_array_ptr<int> q : count(1)) {
  if (p[i]) {
    // Observed bounds context: { p => bounds(p, p + i), q => bounds(q, q + 1) }
    // CHECK: Statement S:
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK:          ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK:            DeclRefExpr {{.*}} 'p'
    // CHECK:          ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK:            DeclRefExpr {{.*}} 'i'
    // CHECK: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: ParmVarDecl {{.*}} p
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: ParmVarDecl {{.*}} q
    // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
    // CHECK-NEXT:     IntegerLiteral {{.*}} 1
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'q'
    // CHECK-NEXT:     IntegerLiteral {{.*}} 1
    // CHECK-NEXT: }

    if (q[1]) {
      // Bounds of p have been widened by 1
      // Observed bounds context: { p => bounds(p, (p + i) + 1), q => bounds(q, q + 1) }
      // CHECK: Statement S:
      // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:   ArraySubscriptExpr
      // CHECK:          ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK:            DeclRefExpr {{.*}} 'q'
      // CHECK:          IntegerLiteral {{.*}} 1
      // CHECK: Observed bounds context after checking S:
      // CHECK-NEXT: {
      // CHECK-NEXT: Variable:
      // CHECK-NEXT: ParmVarDecl {{.*}} p
      // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: RangeBoundsExpr
      // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
      // CHECK-NEXT:   BinaryOperator {{.*}} '+'
      // CHECK-NEXT:     BinaryOperator {{.*}} '+'
      // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
      // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT: Variable:
      // CHECK-NEXT: ParmVarDecl {{.*}} q
      // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: RangeBoundsExpr
      // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:   BinaryOperator {{.*}} '+'
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT: }

      // Bounds of p and q have been widened by 1
      // Observed bounds context: { p => bounds(p, (p + i) + 1), q => bounds(q, (q + 1) + 1) }
      i;
      // CHECK: Statement S:
      // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT: Observed bounds context after checking S:
      // CHECK-NEXT: {
      // CHECK-NEXT: Variable:
      // CHECK-NEXT: ParmVarDecl {{.*}} p
      // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: RangeBoundsExpr
      // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
      // CHECK-NEXT:   BinaryOperator {{.*}} '+'
      // CHECK-NEXT:     BinaryOperator {{.*}} '+'
      // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
      // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT: Variable:
      // CHECK-NEXT: ParmVarDecl {{.*}} q
      // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: RangeBoundsExpr
      // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:   BinaryOperator {{.*}} '+'
      // CHECK-NEXT:     BinaryOperator {{.*}} '+'
      // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:         DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:       IntegerLiteral {{.*}} 1
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT: }

      // This statement kills the widened bounds of p and q
      // Observed bounds context: { p => bounds(p, p + i), q => bounds(q, q + 1) }
      i = 0, q++;
      // CHECK: Statement S:
      // CHECK-NEXT: BinaryOperator {{.*}} ','
      // CHECK-NEXT:   BinaryOperator {{.*}} '='
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT:     IntegerLiteral {{.*}} 0
      // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
      // CHECK: Observed bounds context after checking S:
      // CHECK-NEXT: {
      // CHECK-NEXT: Variable:
      // CHECK-NEXT: ParmVarDecl {{.*}} p
      // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: RangeBoundsExpr
      // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
      // CHECK-NEXT:   BinaryOperator {{.*}} '+'
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT: Variable:
      // CHECK-NEXT: ParmVarDecl {{.*}} q
      // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: RangeBoundsExpr
      // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:   BinaryOperator {{.*}} '+'
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT: }
    }
  }
}
