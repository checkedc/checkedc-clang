// Tests for updating equivalent expression information during bounds inference and checking.
// This file tests updating the context mapping variables to their bounds
// after checking expressions during bounds analysis.
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s

#include <stdchecked.h>

////////////////////////////////////////////////
// No assignments to variables used in bounds //
////////////////////////////////////////////////

// Parameter and local variables with declared count bounds
void declared1(array_ptr<int> arr : count(len), int len, int size) {
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
  // CHECK-NEXT: ParmVarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
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
  // CHECK-NEXT: ParmVarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
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
void declared2(int flag, int x, int y) {
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

/////////////////////////////////////////////
// Assignments to variables used in bounds //
/////////////////////////////////////////////

// Assignment to a variable used in its own bounds
void assign1(array_ptr<int> arr : count(1)) {
  // Original value of arr: arr - 2
  // Observed bounds context: { arr => bounds(arr - 2, (arr - 2) + 1) }
  arr = arr + 2;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// Assignment to a variable used in other variables' bounds
void assign2(array_ptr<int> a : count(len - 1), char b nt_checked[0] : count(len), unsigned len) {
  // Original value of len: len + 3
  // Observed bounds context: { a => bounds(a, a + ((len + 3) - 1)), b => bounds(b, b + (len + 3)) }
  len = len - 3;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:         IntegerLiteral {{.*}} 1
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       BinaryOperator {{.*}} '+'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:           IntegerLiteral {{.*}} 3
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:         IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:         IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }
}

// Assignment to a variable doesn't affect bounds that don't use the variable
void assign3(array_ptr<int> a : bounds(unknown), nt_array_ptr<char> b : count(1), int len) {
  // Observed bounds context: { a => bounds(unknown), b => bounds(b, b + 1) }
  len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// Multiple assignments to variables used in bounds
void assign4(array_ptr<int> a : count(len), unsigned len) {
  // Original value of a: a - 1, original value of len: len + 1
  // Observed bounds context: { a => bounds(a - 1, (a - 1) + (len + 1)) }
  ++a, len--;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
}

// Original value of variable used in bounds is another variable
void assign5(array_ptr<int> a : count(len), int len, int size) {
  // Observed bounds context: { a => bounds(a, a + len) }
  size = len;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }

  // Original value of len: size
  // Observed bounds context: { a => bounds(a, a + size) }
  len = len * 2;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   BinaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT: }
}

// Assignment to a variable with no original value sets the observed bounds
// that use the variable to unknown
void assign6(array_ptr<int> a : count(len), int len) {
  // Original value of len: null
  // Observed bounds context: { a => bounds(unknown) }
  len = len * 2;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   BinaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}
