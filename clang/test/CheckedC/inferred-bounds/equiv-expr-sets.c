// Tests for updating equivalent expression information during bounds inference and checking.
// This file tests updating the set UEQ of sets of equivalent expressions after checking
// initializations and assignments during bounds analysis.
// Updating this set of sets also updates the set G of expressions that produce the same value
// as a given expression. Since the set G is usually included in the set UEQ, this file typically
// does not explicitly test the contents of G (G is tested in equiv-exprs.c).
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s

#include <stdchecked.h>

//////////////////////////////////////////////
// Functions containing a single assignment //
//////////////////////////////////////////////

// VarDecl: initializer
void f1(void) {
  int i = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} i
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// BinaryOperator: non-compound assignment to a variable
void f2(int i) {
  // Updated UEQ: { { 1, i } }
  i = 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// BinaryOperator: compound assignment to a variable
void f3(unsigned i) {
  // Updated UEQ: { { (i - 2) + 2, i } }
  i += 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '+='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// BinaryOperator: non-compound assignment to a non-variable
void f4(ptr<int> p) {
  // Updated UEQ: { { 3, *p } }
  *p = 3;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   UnaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 3
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   UnaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// BinaryOperator: compound assignment to a non-variable
void f5(int arr[1]) {
  // Updated UEQ: { }, Updated G: { arr[0] * 4 }
  arr[0] *= 4;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT:   IntegerLiteral {{.*}} 4
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '*'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     ArraySubscriptExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT:  IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }
}

// UnaryOperator: pre-increment operator on a variable (unsigned integer arithmetic)
void f6(unsigned x) {
  // Updated UEQ: { { (x - 1) + 1, x } }
  ++x;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT:   IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator: pre-decrement operator on a variable (checked pointer arithmetic)
void f7(array_ptr<int> arr) {
  // Updated UEQ: { { (arr + 1) - 1, arr } }
  --arr;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT:   IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator: post-increment operator on a variable (unsigned integer arithmetic)
void f8(unsigned x) {
  // Updated UEQ: { { x - 1, x } }
  x++;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator: post-decrement operator on a variable (checked pointer arithmetic)
void f9(array_ptr<int> arr) {
  // Updated UEQ: { { arr + 1, arr } }
  arr--;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator: pre-increment operator on a variable (float arithmetic)
void f10 (float f) {
  // Updated UEQ: { }, Updated G: { f }
  ++f;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'f'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'f'
  // CHECK-NEXT: }
}

// UnaryOperator: pre-decrement operator on a non-variable
void f11(int *p) {
  // Updated UEQ: { }, Updated G: { *p - 1 }
  --*p;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:   UnaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     UnaryOperator {{.*}} '*'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT: }
}

//////////////////////////////////////////////////////////////
// Functions and statements containing multiple assignments //
//////////////////////////////////////////////////////////////

// Assign one value to each variable.
void f12(int x, int y, int z, int w) {
  // Updated UEQ: { { 1, x }, { 2, y } }
  x = 1, y = 2;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated UEQ: { { 1, x }, { 2, y }, { 3, w, z } }
  z = (w = 3);
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'w'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 3
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'w'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// Overwrite variable values.
void f13(int x, int y) {
  // Updated UEQ: { { 1, x } }
  x = 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated UEQ: { { 1, x, y } }
  y = x;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated UEQ: { { 1, y, y }, { 2, x } }
  x = 2;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}
