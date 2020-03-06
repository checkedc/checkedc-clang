// Tests for updating equivalent expression information during bounds inference and checking.
// This file tests updating the context mapping variables to their bounds
// after checking expressions during bounds analysis.
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s

#include <stdchecked.h>

// Parameter with bounds
void f1(array_ptr<int> arr : count(len), int len, int size) {
  // Updated bounds context: { a => count(5), arr => count(len) }
  array_ptr<int> a : count(5) = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} a
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       IntegerLiteral {{.*}} 5
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK: Bounds:
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT:   IntegerLiteral {{.*}} 5
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} arr
  // CHECK: Bounds:
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }

  // Updated bounds context: { a => count(5), arr => count(len), b => count(size) }
  array_ptr<int> b : count(size) = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} b
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK: Bounds:
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT:   IntegerLiteral {{.*}} 5
  // CHECK: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} arr
  // CHECK: Bounds:
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'len'
  // CHECK: Variable:
  // CHECK-NEXT: VarDecl {{.*}} b
  // CHECK: Bounds:
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT: }
}

// If statement, redeclared variable
void f2(int flag, int x, int y) {
  // Updated bounds context: { a => count(x) }
  array_ptr<int> a : count(x) = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} a
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:         IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK: Bounds:
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }

  if (flag) {
    // Updated bounds context: { a => count(x), a => count(y) }
    array_ptr<int> a : count(y) = 0;
    // CHECK: Statement S:
    // CHECK:      DeclStmt
    // CHECK-NEXT:   VarDecl {{.*}} a
    // CHECK-NEXT:     CountBoundsExpr
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <NullToPointer>
    // CHECK-NEXT:         IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: VarDecl {{.*}} a
    // CHECK: Bounds:
    // CHECK-NEXT: CountBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
    // CHECK: Variable:
    // CHECK-NEXT: VarDecl {{.*}} a
    // CHECK: Bounds:
    // CHECK-NEXT: CountBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: }

    // Updated bounds context: { a => count(x), a => count(y), b => count(y) }
    array_ptr<int> b : count(y) = 0;
    // CHECK: Statement S:
    // CHECK:      DeclStmt
    // CHECK-NEXT:   VarDecl {{.*}} b
    // CHECK-NEXT:     CountBoundsExpr
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <NullToPointer>
    // CHECK-NEXT:         IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: Variable:
    // CHECK-NEXT: VarDecl {{.*}} a
    // CHECK: Bounds:
    // CHECK-NEXT: CountBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
    // CHECK: Variable:
    // CHECK-NEXT: VarDecl {{.*}} a
    // CHECK: Bounds:
    // CHECK-NEXT: CountBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
    // CHECK: Variable:
    // CHECK-NEXT: VarDecl {{.*}} b
    // CHECK: Bounds:
    // CHECK-NEXT: CountBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: }
  }

  // Updated bounds context: { a => count(x), c => count(x) }
  array_ptr<int> c : count(x) = a;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} c
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK: Bounds:
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK: Variable:
  // CHECK-NEXT: VarDecl {{.*}} c
  // CHECK: Bounds:
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
}
