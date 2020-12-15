// Tests for updating equivalent expression information during bounds inference and checking.
// This file tests updating the set EquivExprs of sets of equivalent expressions after checking
// initializations and assignments during bounds analysis.
// Updating this set of sets also updates the set SameValue of expressions that produce the same value
// as a given expression. Since the set SameValue is usually included in the set EquivExprs, this file typically
// does not explicitly test the contents of SameValue (SameValue is tested in equiv-exprs.c).
//
// For readability, the expected EquivExprs sets are commented before assignments. These comments omit
// certain casts to improve readability. All variables in the comments should be read as operands
// of an implicit LValueToRValue cast. For example, the comment "Updated EquivExprs: { { x, y } }" should be
// read as "Updated EquivExprs: { { LValueToRValue(x), LValueToRValue(y) } }".
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s

#include <stdchecked.h>

extern array_ptr<int> g1(array_ptr<int> arr : count(1)) : count(1);

//////////////////////////////////////////////
// Functions containing a single assignment //
//////////////////////////////////////////////

// VarDecl: initializer
void vardecl1(void) {
  // Updated EquivExprs: { { 0, i } }
  int i = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} i
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// BinaryOperator: assignments to variables where the variable
// does not appear on the right-hand side
void binary1(int i, nt_array_ptr<char> c) {
  // Updated EquivExprs: { }
  c = "abc";
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'char [4]'
  // CHECK-NEXT:       StringLiteral {{.*}} "abc"
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }

  // Updated EquivExprs: { { 1, i } }
  i = 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// BinaryOperator: assignments to variables where the variable
// appears on the right-hand side
void binary2(unsigned i) {
  // Updated EquivExprs: { }, Updated SameValue: { i }
  i = i + 2;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Updated EquivExprs: { }, Updated SameValue: { i }
  i += 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '+='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// BinaryOperator: non-compound assignments to non-variables
// (non-modifying and modifying expressions)
void binary3(int arr[1], int i) {
  // Updated EquivExprs: { }, Updated SameValue: { }
  *arr = 3;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   UnaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 3
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }

  // Updated EquivExprs: { }, Updated SameValue: { }
  arr[i++] = 3;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:  IntegerLiteral {{.*}} 3
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}

// BinaryOperator: compound assignments to non-variables
// (non-modifying and modifying expressions)
void binary4(int arr[1], int i) {
  // Updated EquivExprs: { }, Updated SameValue: { }
  arr[0] *= 4;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT:   IntegerLiteral {{.*}} 4
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }

  // Updated EquivExprs: { }, Updated SameValue: { }
  arr[--i] += 4;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '+='
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 4
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}

// UnaryOperator: pre-increment operator on a variable (unsigned integer arithmetic)
void unary1(unsigned x) {
  // Updated EquivExprs: { }, Updated SameValue: { x }
  ++x;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
}

// UnaryOperator: pre-decrement operator on a variable (checked pointer arithmetic)
void unary2(array_ptr<int> arr) {
  // Updated EquivExprs: { }, Updated SameValue: { arr }
  --arr;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
}

// UnaryOperator: post-increment operator on a variable (unsigned integer arithmetic)
void unary3(unsigned x) {
  // Updated EquivExprs: { }, Updated SameValue: { x - 1 }
  x++;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// UnaryOperator: post-decrement operator on a variable (checked pointer arithmetic)
void unary4(array_ptr<int> arr) {
  // Updated EquivExprs: { }, Updated SameValue: { arr + 1 }
  arr--;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// UnaryOperator: pre-increment operator on a variable (float arithmetic)
void unary5(float f) {
  // Updated EquivExprs: { }, Updated SameValue: { }
  ++f;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'f'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}

// UnaryOperator: post-decrement operator on a non-variable
void unary6(int *p) {
  // Updated EquivExprs: { }, Updated SameValue: { (*p) + 1 }
  (*p)--;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     UnaryOperator {{.*}} '*'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       UnaryOperator {{.*}} '*'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 'int' 1
  // CHECK-NEXT: }
}

//////////////////////////////////////////////////////////////
// Functions and statements containing multiple assignments //
//////////////////////////////////////////////////////////////

// Assign one value to each variable
void multiple_assign1(int x, int y, int z, int w) {
  // Updated EquivExprs: { { 1, x }, { 2, y } }
  x = 1, y = 2;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
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

  // Updated EquivExprs: { { 1, x }, { 2, y }, { 3, w, z } }
  z = (w = 3);
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'w'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK: Sets of equivalent expressions after checking S:
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

// Overwrite variable values
void multiple_assign2(int x, int y) {
  // Updated EquivExprs: { { 1, x } }
  x = 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { { 1, x, y } }
  y = x;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { { 1, y }, { 2, x } }
  x = 2;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
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

// Equivalence resulting from assignments to 0 (including NullToPointer casts)
void multiple_assign3(int i, int *p, ptr<int> pt, array_ptr<int> a, array_ptr<int> b) {
  // Updated EquivExprs: { { 0, i } }
  i = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { { 0, i, p } }
  p = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { { 0, i, p, a } }
  a = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { { 0, i, p, a, b } }
  b = a;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

///////////////////////////////////////////
// Assignments involving original values // 
///////////////////////////////////////////

// UnaryOperator: '+' inverse
void original_value1(int x, int i) {
  // Updated EquivExprs: { { i, x } }, Updated SameValue: { i, x }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }

  // Original value of i in +i: +i
  // Updated EquivExprs: { { +i, x } }, Updated SameValue: { i }
  i = +i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   UnaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// UnaryOperator: '-' inverse
void original_value2(int x, int i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in -i: -i
  // Updated EquivExprs: { { -i, x } }, Updated SameValue: { i }
  i = -i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   UnaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// UnaryOperator: '~' inverse
void original_value3(int x, int i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in ~i: ~i
  // Updated EquivExprs: { { ~i, x } }, Updated SameValue: { i }
  i = ~i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   UnaryOperator {{.*}} '~'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '~'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// BinaryOperator: '^' inverse
void original_value4(int x, int i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in 2 ^ i: i ^ 2
  // Updated EquivExprs: { { i ^ 2, x } }, Updated SameValue: { i }
  i = 2 ^ i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '^'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '^'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// BinaryOperator: '-' inverse
void original_value5(unsigned i, unsigned x) {
  // Updated EquivExprs: { { x, i } }
  i = x;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in i - x: i + x
  // Updated EquivExprs: { { x, i + x } }, Updated SameValue: { i }
  // Note: since i == x before this assignment, i is equivalent
  // to 0 after this assignment, so recording equality between x and i + x
  // is mathematically equivalent to recording equality between x and x.
  i = i - x;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// Combined UnaryOperator and BinaryOperator inverse
void original_value6(unsigned x, unsigned i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in -(i + 2): -i - 2
  // Updated EquivExprs: { { -i - 2, x } }, Updated SameValue: { i }
  i = -(i + 2);
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   UnaryOperator {{.*}} '-'
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       BinaryOperator {{.*}} '+'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:           IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   UnaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// Combined BinaryOperator and UnaryOperator inverse
void original_value7(unsigned x, unsigned i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in ~i + 3: ~(i - 3)
  // Updated EquivExprs: { { ~(i - 3), x } }, Updated SameValue: { i }
  i = ~i + 3;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     UnaryOperator {{.*}} '~'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '~'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// UnaryOperator: &* inverse
void original_value8(array_ptr<int> p) {
  // Updated EquivExprs: { { p, q } }
  array_ptr<int> q = p;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} q
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of p in &*p: p
  // Updated EquivExprs: { { p, q } }
  p = &*p;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   UnaryOperator {{.*}} '&'
  // CHECK-NEXT:     UnaryOperator {{.*}} '*'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of p in &(*(p + 1)): p - 1
  // Updated EquivExprs: { { p - 1, q } }
  p = &(*(p + 1));
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   UnaryOperator {{.*}} '&'
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       UnaryOperator {{.*}} '*'
  // CHECK-NEXT:         ParenExpr
  // CHECK-NEXT:           BinaryOperator {{.*}} '+'
  // CHECK-NEXT:             ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:               DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:             IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator: *& inverse
void original_value9(int x, int y) {
  // Updated EquivExprs: { { x, y } }
  y = x;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of x in (*((&(x)))): x
  // Updated EquivExprs: { { x, y } }
  x = (*((&(x))));
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       UnaryOperator {{.*}} '*'
  // CHECK-NEXT:         ParenExpr
  // CHECK-NEXT:           ParenExpr
  // CHECK-NEXT:             UnaryOperator {{.*}} '&'
  // CHECK-NEXT:               ParenExpr
  // CHECK-NEXT:                 DeclRefExpr {{.*}} 'x'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator: & and * with no inverse
void original_value10(int *p, int *q, int *r, int *s) {
  // Updated EquivExprs: { { p + 1, q }, { r + 1, s } }
  q = p + 1, s = r + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 's'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'r'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'r'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of p in &*&p: null
  // Updated EquivExprs: { { r + 1, s } }
  p = &*&p;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'int *' <BitCast>
  // CHECK-NEXT:     UnaryOperator {{.*}} '&'
  // CHECK-NEXT:       UnaryOperator {{.*}} '*'
  // CHECK-NEXT:         UnaryOperator {{.*}} '&'
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'r'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 's'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of r in *r: null
  // Updated EquivExprs: { }
  r = *r;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'r'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'int *' <IntegralToPointer>
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'int' <LValueToRValue>
  // CHECK-NEXT:       UnaryOperator {{.*}} '*'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} 'int *' <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'r'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// UnaryOperator and ArraySubscriptExpr inverse
void original_value11(array_ptr<int> p, array_ptr<int> q) {
  // Updated EquivExprs: { { p + 1, q } }
  q = p + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of p in (_Array_ptr<int>&p[2]): (int *)p - 2
  // Updated EquivExprs: { { (int *)p - 2 + 1, q } }
  p = &p[2];
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} '_Array_ptr<int>' <BitCast>
  // CHECK-NEXT:     UnaryOperator {{.*}} 'int *' prefix '&'
  // CHECK-NEXT:       ArraySubscriptExpr {{.*}} 'int'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'int *' <BitCast>
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator and ArraySubscriptExpr: no inverse (unchecked pointer arithmetic)
void original_value12(int *p, int *q) {
  // Updated EquivExprs: { { p + 1, q } }
  q = p + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of p in &p[0]: null
  // Updated EquivExprs: { { } }
  p = &p[0];
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   UnaryOperator {{.*}} 'int *' prefix '&'
  // CHECK-NEXT:     ArraySubscriptExpr {{.*}} 'int'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// UnaryOperator and ArraySubscriptExpr: combined &* and array subscript inverse
void original_value13(array_ptr<int> p, array_ptr<int> q) {
  // Updated EquivExprs: { { p + 1, q } }
  q = p + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of p in (_Array_ptr<int>)&0[&*p]: (int *)p - 0
  // Updated EquivExprs: { { (int *)p - 0 + 1, q } }
  p = &0[&*p];
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} '_Array_ptr<int>' <BitCast>
  // CHECK-NEXT:     UnaryOperator {{.*}} 'int *' prefix '&'
  // CHECK-NEXT:       ArraySubscriptExpr {{.*}} 'int'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 0
  // CHECK-NEXT:         UnaryOperator {{.*}} '_Array_ptr<int>' prefix '&'
  // CHECK-NEXT:           UnaryOperator {{.*}} '*'
  // CHECK-NEXT:             ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:               DeclRefExpr {{.*}} 'p'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'int *' <BitCast>
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'q'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator: pre-increment and post-increment inverses (unsigned integer arithmetic)
void original_value14(unsigned x, unsigned i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in i + 1: i - 1
  // Updated EquivExprs: { { i - 1, x } }, Updated SameValue: { i }
  ++i;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Original value of i in i + 1: i - 1
  // Updated EquivExprs: { { i - 1 - 1, x } }, Updated SameValue: { i - 1 }
  i++;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// UnaryOperator: pre-decrement and post-decrement inverses (checked pointer arithmetic)
void original_value15(array_ptr<float> x, array_ptr<float> i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in i - 1: i + 1
  // Updated EquivExprs: { { i + 1, x } }, Updated SameValue: { i }
  --i;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Original value of i in i - 1: i + 1
  // Updated EquivExprs: { { i + 1 + 1, x } }, Updated SameValue: { i + 1 }
  i--;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// UnaryOperator: pre-increment with no inverse (signed integer arithmetic)
void original_value16(int x, int i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in i + 1: x
  // Updated EquivExprs: { { x + 1, i } }, Updated SameValue: { i }
  ++i;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// UnaryOperator: post-increment with no inverse (unchecked pointer arithmetic)
void original_value17(unsigned *x, unsigned *i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in i + 1: x
  // Updated EquivExprs: { { x + 1, i } }, Updated SameValue: { i - 1 }
  i++;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// UnaryOperator: pre-decrement with no inverse (unchecked pointer arithmetic)
void original_value18(float *x, float *i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in i - 1: x
  // Updated EquivExprs: { { x - 1, i } }, Updated SameValue: { i }
  --i;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// UnaryOperator: post-decrement with no inverse (unchecked pointer arithmetic)
void original_value19(array_ptr<int> *x, array_ptr<int> *i) {
  // Updated EquivExprs: { { i, x } }
  x = i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in i - 1: x
  // Updated EquivExprs: { { x - 1, i } }, Updated SameValue: { i + 1 }
  i--;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// No inverse
void original_value20(unsigned x, unsigned i, unsigned *p, unsigned arr[1]) {
  // Updated EquivExprs: { { i * 1, x } }, Updated SameValue: { i * 1, x }
  x = i * 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   BinaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '*'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '*'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }

  // Updated EquivExprs: { }, Updated SameValue: { i }
  i = i + i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Updated EquivExprs: { }, Updated SameValue: { i }
  i = 2 * i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Updated EquivExprs: { }, Updated SameValue: { i }
  i += *p;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '+='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     UnaryOperator {{.*}} '*'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Updated EquivExprs: { }, Updated SameValue: { i }
  i -= arr[0];
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '-='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     ArraySubscriptExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// Original value from equivalence with another variable
void original_value21(int x, int y) {
  // Updated EquivExprs: { { y, x } }
  x = y;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of x in x * 2: y
  // Updated EquivExprs: { { y * 2, x } }, Updated SameValue: { y * 2, x }
  x *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '*'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '*'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
}

// Original value from equivalence with another variable,
// accounting for value-preserving casts when searching for a set
// in EquivExprs that contains a variable w != v in an assignment v = src
void original_value22(array_ptr<int> arr_int, array_ptr<const int> arr_const, array_ptr<int> arr) {
  // Updated EquivExprs: { { arr_int + 1, arr } }
  arr = arr_int + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { { arr_int + 1, arr } { (array_ptr<const int>)arr_int, arr_const } }
  arr_const = arr_int;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_const'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} '_Array_ptr<const int>' <NoOp>
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr_int'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} '_Array_ptr<const int>' <NoOp>
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_const'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of arr_int: arr_const
  // Updated EquivExprs: { { arr_const + 1, arr }, { NullToPointer(0), arr_int } }
  arr_int = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr_const'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT: }
}

// Original value is a value-preserving cast of another variable
void original_value23(array_ptr<int> arr_int, array_ptr<const int> arr_const, array_ptr<int> arr) {
  // Updated EquivExprs: { { (array_ptr<int>)arr_const, arr_int } }
  arr_int = arr_const;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} '_Array_ptr<int>' <NoOp>
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr_const'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} '_Array_ptr<int>' <NoOp>
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr_const'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { { (array_ptr<int>)arr_const, arr_int }, { arr_int + 1, arr } }
  arr = arr_int + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} '_Array_ptr<int>' <NoOp>
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr_const'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of arr_int: (array_ptr<int>)arr_const
  // Updated EquivExprs: { { (array_ptr<int>)arr_const + 1, arr }, { NullToPointer(0), arr_int } }
  arr_int = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} '_Array_ptr<int>' <NoOp>
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr_const'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_int'
  // CHECK-NEXT: }
}

// The left-hand side variable is the original value
void original_value24(int x, int val) {
  // Updated EquivExprs: { { x + 1, val } }
  val = x + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: } 

  // Original value of x in x: x
  // Updated EquivExprs: { { x + 1, val } }
  x = x;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: } 
}

// Original value is a no-op cast
void original_value25(int i) {
  // Updated EquivExprs: { { i + 1, val } }
  int val = i + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} val
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in (int)i: (int)i
  // Updated EquivExprs: { { (int)i + 1, val } }
  i = (int)i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   CStyleCastExpr {{.*}} 'int' <NoOp>
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   CStyleCastExpr {{.*}} 'int' <NoOp>
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// Original value involves explicit and implicit casts
void original_value26(int i, int val) {
  // Updated EquivExprs: { { i + 1, val } }
  val = i + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in (int)(unsigned int)i: (int)(unsigned int)i
  // Updated EquivExprs: { { (int)(unsigned int)i + 1, val }, { i, x } }
  int x = (i = (unsigned)i);
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} x
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} 'int' <IntegralCast>
  // CHECK-NEXT:           CStyleCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:             ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:               DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   CStyleCastExpr {{.*}} 'int' <IntegralCast>
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// Combined CastExpr and BinaryOperator inverse
void original_value27(int i, unsigned j, int val) {
  // Updated EquivExprs: { { i + 1, val } }
  val = i + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in (int)((unsigned int)i + j): (int)((unsigned int)i - j)
  // Updated EquivExprs: { { (int)((unsigned int)i - j) + 1, RValue(val) } }
  i = i + j;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'int' <IntegralCast>
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'j'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'int' <IntegralCast>
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// No original value for an implicit narrowing cast
void original_value28(int i, float f, int val) {
  // Updated EquivExprs: { { i + 1, val } }
  val = i + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of i in i + f: null (the implicit FloatingToIntegral cast is narrowing)
  // Updated EquivExprs: { }
  i = i + f;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'int' <FloatingToIntegral>
  // CHECK-NEXT:     BinaryOperator {{.*}} 'float' '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} 'float' <IntegralToFloating>
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'f'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// No original value for an explicit narrowing cast
void original_value29(double d, double val) {
  // Updated EquivExprs: { { d + 1.0, val } }
  val = d + 1.0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:     FloatingLiteral {{.*}} 1.0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:   FloatingLiteral {{.*}} 1.0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of d in (int)d: null (the explicit FloatingToIntegral cast is narrowing)
  // Updated EquivExprs: { }
  d = (int)d;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'double' <IntegralToFloating>
  // CHECK-NEXT:     CStyleCastExpr {{.*}} 'int' <FloatingToIntegral>
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} 'double' <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'd'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// Bounds cast expression inverse where the type is the same as the LHS variable type
void original_value30(array_ptr<int> a : count(1), array_ptr<int> val) {
  // Updated UEQ: { { a + 1, val } }
  val = a + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of a: (array_ptr<int>)a
  // Updated EquivExprs: { { (array_ptr<int>)a + 1, val } }
  a = _Dynamic_bounds_cast<array_ptr<int>>(a, count(1));
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BoundsCastExpr {{.*}} '_Array_ptr<int>' <DynamicPtrBounds>
  // CHECK:          CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   CStyleCastExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }  
}

// Bounds cast expression inverse including a type cast
void original_value31(array_ptr<int> a : count(1), array_ptr<int> val) {
  // Updated UEQ: { { a + 1, val } }
  val = a + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of a: (array_ptr<int>)(array_ptr<const int>)a
  // Updated EquivExprs: { { (array_ptr<int>)(array_ptr<const int>)a + 1, val } }
  a = _Assume_bounds_cast<array_ptr<const int>>(a, count(1));
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} '_Array_ptr<int>' <NoOp>
  // CHECK-NEXT:     BoundsCastExpr {{.*}} '_Array_ptr<const int>' <AssumePtrBounds>
  // CHECK:            CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:         IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   CStyleCastExpr {{.*}} '_Array_ptr<int>' <BitCast>
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} '_Array_ptr<const int>' <NoOp>
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }  
}

// Combined bounds cast and binary inverse
void original_value32(array_ptr<int> a : count(1), array_ptr<int> val) {
  // Updated UEQ: { { a + 1, val } }
  val = a + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of a: (array_ptr<int>)(array_ptr<const int>)a - 1
  // Updated EquivExprs: { { (array_ptr<int>)(array_ptr<const int>)a - 1 + 1, val }, { value(temp(a + 1)), a } }
  // TODO: checkedc-clang issue #832: equality between value(temp(a + 1)) and a should not be recorded
  a = _Dynamic_bounds_cast<array_ptr<const int>>(a + 1, count(1));
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} '_Array_ptr<int>' <NoOp>
  // CHECK-NEXT:     BoundsCastExpr {{.*}} '_Array_ptr<const int>' <DynamicPtrBounds>
  // CHECK:            CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:         BinaryOperator {{.*}} '+'
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 1
  // CHECK-NEXT:       CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:         IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     CStyleCastExpr {{.*}} '_Array_ptr<int>' <BitCast>
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} '_Array_ptr<const int>' <NoOp>
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: }  
}

// CallExpr: using the left-hand side of an assignment as a call argument
void original_value33(array_ptr<int> a : count(1), array_ptr<int> b : count(1)) {
  // Updated UEQ: { { b, a } }
  a = b;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of a in g1(a): b
  // Updated EquivExprs: { { g1(a), a } }
  // Note that a is not replaced with b in g1(a)
  a = g1(a);
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     CallExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'g1'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// The original value is type-compatible with the variable,
// even when expressions in the same set are not type compatible
// (resulting from assignments to NullToPointer casts)
void original_value34(int *p,
                      array_ptr<int> val, array_ptr<int> arr,
                      array_ptr<int> a, array_ptr<int> b,
                      array_ptr<char> c, nt_array_ptr<int> n) {
  // Updated EquivExprs: { { NullToPointer(0), p } }
  p = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { NullToPointer(0), p, c }
  c = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { NullToPointer(0), p, c, n }
  n = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'n'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'n'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { NullToPointer(0), p, c, n, b, a }
  b = 0, a = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'n'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Updated EquivExprs: { { NullToPointer(0), p, c, n, b, a }, { a + 1, val } }
  val = a + 1;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'n'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of a: b
  // Note: p, c, and n are not the original value of a since their types are
  // not compatible with the type of a
  // Updated EquivExprs: { { NullToPointer(0), p, c, n, b }, { b + 1, val }, { arr, a } }
  a = arr;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'n'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'val'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

/////////////////////////////////
// Functions with control flow //
/////////////////////////////////

// If statement: assignment that affects equality sets
void control_flow1(int flag, int i, int j) {
  // Updated EquivExprs: { { i, len } }
  int len = i;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  if (flag) {
    // Current EquivExprs: { { i, len } }
    // Original value of len in j: i
    // Updated EquivExprs: { { j, len } }
    len = j;
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'j'
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  }

  // Current EquivExprs: { }
  // Original value of len in len * 2: null
  // Updated EquivExprs: { }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// If statement: assignment that does not affect equality sets
void control_flow2(int flag, int i, int j) {
  // Updated EquivExprs: { { i, len } }
  int len = i;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  if (flag) {
    // Current EquivExprs: { { i, len } }
    // Updated EquivExprs: { { i, len }, { 42, j } }
    j = 42;
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT:   IntegerLiteral {{.*}} 42
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 42
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  }

  // Current EquivExprs: { { i, len } }
  // Original value of len in len * 2: i
  // Updated EquivExprs: { { i * 2, len } }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '*'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// If/else statements: different assignments
void control_flow3(int flag, int i, int j, int k) {
  // Updated EquivExprs: { { i, len } }
  int len = i;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Note: the "else" block is traversed before the "if" block
  // so the expected statement outputs are not in the same order
  // as the statements appear in code.

  if (flag) {
    // Current EquivExprs: { { i, len } }
    // Original value for len in j: i
    // Updated EquivExprs: { { j, len } }
    len = j;
    // Expected output of len = k from the "else" block
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'k'
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'k'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  } else {
    // Current EquivExprs: { { i, len } }
    // Original value for len in k: i
    // Updated EquivExprs: { { k, len } }
    len = k;
    // Expected output of len = j from the "if" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'j'
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  }

  // Current EquivExprs: { }
  // Original value of len in 2 * len: null
  // Updated EquivExprs: { }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// If/else statements: more complicated intersection of equality sets
void control_flow4(int flag, int x, int y, int z, int w) {
  // Updated EquivExprs: { { 0, len } }
  int len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Note: the "else" block is traversed before the "if" block
  // so the expected statement outputs are not in the same order
  // as the statements appear in code.

  if (flag) {
    // Current EquivExprs: { { 0, len } }
    // Updated EquivExprs: { { 0, len }, { 2, y } }
    y = 2;
    // Expected output of x = 3 from the "else" block
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT:   IntegerLiteral {{.*}} 3
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 0
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 3
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT: }
    // CHECK-NEXT: }

    // Updated EquivExprs: { { 0, len }, { 2, y, x } }
    x = y;
    // Expected output of y = x from the "else" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 0
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 3
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: }

    // Updated EquivExprs: { { 0, len }, { 2, y, x }, { w, z } }
    z = w;
    // Expected output of w = z from the "else" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'w'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'z'
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 0
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 3
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: }
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'w'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  } else {
    // Current EquivExprs: { { 0, len } }
    // Updated EquivExprs: { { 0, len }, { 3, x } }
    x = 3;
    // Expected output of y = 2 from the "if" block
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:   IntegerLiteral {{.*}} 2
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 0
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 2
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: }
    // CHECK-NEXT: }

    // Updated EquivExprs: { { 0, len }, { 3, x, y } }
    y = x;
    // Expected output of x = y from the "if" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 0
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 2
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT: }

    // Updated EquivExprs: { { 0, len }, { 3, x, y }, { z, w } }
    w = z;
    // Expected output of z = w from the "if" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'w'
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 0
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: {
    // CHECK-NEXT: IntegerLiteral {{.*}} 2
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT: }
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'w'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  }

  // Current EquivExprs: { { 0, len }, { y, x }, { w, z } }
  // Original value of len in len * 2: null
  // Updated EquivExprs: { { y, x }, { w, z } }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'w'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// If/else statements: unreachable else
void control_flow5(int flag, int i, int j) {
  // Updated EquivExprs: { { 0, len } }
  int len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  if (1) {
    // Current EquivExprs: { { 0, len } }
    // Updated EquivExprs: { { i, len } }
    len = i;
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
    // CHECK: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  } else {
    // Unreachable
    len = j;
  }

  // Current EquivExprs: { { i, len } }
  // Original value of len in len * 2: i
  // Updated EquivExprs: { { i * 2, len } }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '*'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

///////////////////////////
// Conditional operators //
///////////////////////////

void conditional1(int x, int y, int z) {
  // No assignments in either arm
  // Updated EquivExprs: { { y, x } }
  (x = y) ? 1 : 2;
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Different assignments in each arm
  // Updated EquivExprs: { { 1, x } }
  (x = 1) ? (y = 2) : (z = 3);
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }

  // Same assignments in each arm
  // Updated EquivExprs: { { 1, x, y }, { 2, z } }
  (x = 1) ? (y = 1, z = 2) : (z = 2, y = 1);
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} ','
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 1
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} ','
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 2
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 1
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:    DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// Variables are overwritten in each conditional arm
void conditional2(int x) {
  // Overwritten with the same value
  // Updated EquivExprs: { 2, x }
  (x = 1) ? (x = 2) : (x = 2);
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Overwritten with different values
  // Updated EquivExprs: { }
  (x = 1) ? (x = 2) : (x = 3);
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// Conditional operators are added to the correct set in EquivExprs
void conditional3(int flag, int x, int y, int z) {
  int i = 0;
  int a = 1;
  x = 1;
  y = 1;

  // Updated EquivExprs: { { 0, i }, { 1, a, x, y, flag ? x : y, v } }
  int v = flag ? x : y;
  // CHECK: Statement S:
  // CHECK: DeclStmt
  // CHECK: VarDecl {{.*}} v
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'flag'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'flag'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'v'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// Nested conditional expressions
void conditional4(int a, int b, int x, int i) {
  int y = 0;
  int z = 0;
  int j = 0;
  int k = 0;

  // Updated EquivExprs: { { 0, y, z, j, k, b ? (x ? y : z) : (i ? j : k), a } }
  a = (b ? (x ? y : z) : (i ? j : k));
  // CHECK: Statement S:
  // CHECK: BinaryOperator {{.*}} '='
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: ParenExpr
  // CHECK-NEXT:   ConditionalOperator
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ParenExpr {{.*}}
  // CHECK-NEXT:       ConditionalOperator
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       ConditionalOperator
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'k'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'k'
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   ParenExpr {{.*}}
  // CHECK-NEXT:     ConditionalOperator
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     ConditionalOperator
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'k'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  y = 1, z = 1;

  // Original value of y and z: j
  // Updated EquivExprs: { { 0, j, k, b ? (x ? j : j) : (i ? j : k) }, 
  //                       { 1, y, z, x ? y : z, b ? (x ? y : z) : (x ? y : z), a } }
  a = (b ? (x ? y : z) : (x ? y : z));
  // CHECK: Statement S:
  // CHECK: BinaryOperator {{.*}} '='
  // CHECK:      DeclRefExpr {{.*}} 'a'
  // CHECK:      ParenExpr
  // CHECK-NEXT:   ConditionalOperator
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ParenExpr {{.*}}
  // CHECK-NEXT:       ConditionalOperator
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       ConditionalOperator
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'z'
  // CHECK: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'k'
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   ParenExpr {{.*}}
  // CHECK-NEXT:     ConditionalOperator
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     ConditionalOperator
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'k'
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 1
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   ParenExpr {{.*}}
  // CHECK-NEXT:     ConditionalOperator
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     ConditionalOperator
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'z'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}
