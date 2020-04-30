// Tests for updating equivalent expression information during bounds inference and checking.
// This file tests updating the context mapping variables to their bounds
// after checking expressions during bounds analysis.
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s

#include <stdchecked.h>

// Parameter with bounds
void f1(array_ptr<int> arr : count(len), int len, int size) {
  // Observed bounds context: { a => bounds(any), arr => bounds(arr, arr + len) }
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
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
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

  // Observed bounds context: { a => bounds(a, a + 5), arr => bounds(arr, arr + len), b => bounds(any) }
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'size'
  // CHECK: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
  // CHECK-NEXT: }
}

// If statement, redeclared variable
void f2(int flag, int x, int y) {
  // Observed bounds context: { a => bounds(any) }
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
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
  // CHECK-NEXT: }

  if (flag) {
    // Observed bounds context: { a => bounds(a, a + x), a => bounds(any) }
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
    // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
    // CHECK-NEXT: }

    // Observed bounds context: { a => bounds(a, a + x), a => bounds(a, a + y), b => bounds(any) }
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
    // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
    // CHECK-NEXT: }
  }

  // Observed bounds context: { a => bounds(a, a + x), c => bounds(a, a + x) }
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
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
}
