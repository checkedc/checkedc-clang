// Tests for updating equivalent expression information during bounds inference and checking.
// This file tests updating the set UEQ of sets of equivalent expressions after checking
// initializations and assignments during bounds analysis.
// Updating this set of sets also updates the set G of expressions that produce the same value
// as a given expression. Since the set G is usually included in the set UEQ, this file typically
// does not explicitly test the contents of G (G is tested in equiv-exprs.c).
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state %s | FileCheck %s

#include <stdchecked.h>

extern array_ptr<int> g1(array_ptr<int> arr : count(1)) : count(1);

//////////////////////////////////////////////
// Functions containing a single assignment //
//////////////////////////////////////////////

// VarDecl: initializer
void vardecl1(void) {
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
void binary1(int i, nt_array_ptr<char> c) {
  // Updated UEQ: { }
  c = "abc";
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'char [4]'
  // CHECK-NEXT:       StringLiteral {{.*}} "abc"
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }

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
  // Updated UEQ: { { (i - 2) + 2, i } }
void binary2(unsigned i) {
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

// BinaryOperator: non-compound assignments to non-variables
// (non-modifying and modifying expressions)
void binary3(int arr[1], int i) {
  // Updated UEQ: { }, Updated G: { }
  *arr = 3;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   UnaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }

  // Updated UEQ: { }, Updated G: { }
  arr[i++] = 3;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:  IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}

// BinaryOperator: compound assignments to non-variables
// (non-modifying and modifying expressions)
void binary4(int arr[1], int i) {
  // Updated UEQ: { }, Updated G: { }
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
  // CHECK-NEXT: { }

  // Updated UEQ: { }, Updated G: { }
  arr[--i] += 4;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '+='
  // CHECK-NEXT:   ArraySubscriptExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 4
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}

// UnaryOperator: pre-increment operator on a variable (unsigned integer arithmetic)
void unary1(unsigned x) {
  // Updated UEQ: { }, Updated G: { x + 1 }
  ++x;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT: }
}

// UnaryOperator: pre-decrement operator on a variable (checked pointer arithmetic)
void unary2(array_ptr<int> arr) {
  // Updated UEQ: { }, Updated G: { arr - 1 }
  --arr;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-NEXT: }
}

// UnaryOperator: post-increment operator on a variable (unsigned integer arithmetic)
void unary3(unsigned x) {
  // Updated UEQ: { }, Updated G: { x }
  x++;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
}

// UnaryOperator: post-decrement operator on a variable (checked pointer arithmetic)
void unary4(array_ptr<int> arr) {
  // Updated UEQ: { }, Updated G: { arr }
  arr--;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT: }
}

// UnaryOperator: pre-increment operator on a variable (float arithmetic)
void unary5(float f) {
  // Updated UEQ: { }, Updated G: { }
  ++f;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'f'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: { }
}

// UnaryOperator: pre-decrement operator on a non-variable
void unary6(int *p) {
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
  // CHECK-NEXT:   IntegerLiteral {{.*}} 'int' 1
  // CHECK-NEXT: }
}

//////////////////////////////////////////////////////////////
// Functions and statements containing multiple assignments //
//////////////////////////////////////////////////////////////

// Assign one value to each variable.
void multiple_assign1(int x, int y, int z, int w) {
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
void multiple_assign2(int x, int y) {
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

  // Updated UEQ: { { 1, y }, { 2, x } }
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
  // CHECK-NEXT: }
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

///////////////////////////////////////////
// Assignments involving original values // 
///////////////////////////////////////////

// UnaryOperator: '+' inverse
void original_value1(int i) {
  // Original value of i in +i: +i
  // Updated UEQ: { { +(+i), i } }
  i = +i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   UnaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '+'
  // CHECK-NEXT:   UnaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator: '-' inverse
void original_value2(int i) {
  // Original value of i in -i: -i
  // Updated UEQ: { { -(-i), i } }
  i = -i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   UnaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '-'
  // CHECK-NEXT:   UnaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// UnaryOperator: '~' inverse
void original_value3(int i) {
  // Original value of i in ~i: ~i
  // Updated UEQ: { { ~(~i), i } }
  i = ~i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   UnaryOperator {{.*}} '~'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '~'
  // CHECK-NEXT:   UnaryOperator {{.*}} '~'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// BinaryOperator: '^' inverse
void original_value4(int i) {
  // Original value of i in 2 ^ i: i ^ 2
  // Updated UEQ: { { 2 ^ (i ^ 2), i } }
  i = 2 ^ i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '^'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '^'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT:   BinaryOperator {{.*}} '^'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// BinaryOperator: '+' inverse
void original_value5(unsigned i, unsigned j) {
  // Original value of i in i - j: i + j
  // Updated UEQ: { { (i + j) - j, i } }
  i = i - j;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '-'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'j'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// Combined UnaryOperator and BinaryOperator inverse
void original_value6(unsigned i) {
  // Original value of i in -(i + 2): -i - 2
  // Updated UEQ: { { -((-i - 2) + 2), i } }
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
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: UnaryOperator {{.*}} '-'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       BinaryOperator {{.*}} '-'
  // CHECK-NEXT:         UnaryOperator {{.*}} '-'
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:           IntegerLiteral {{.*}} 2
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:         IntegerLiteral {{.*}} 2
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// Combined BinaryOperator and UnaryOperator inverse
void original_value7(unsigned i) {
  // Original value of i in ~i + 3: ~(i - 3)
  // Updated UEQ: { { ~(~(i - 3)) + 3, i } }
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
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-NEXT:   UnaryOperator {{.*}} '~'
  // CHECK-NEXT:     UnaryOperator {{.*}} '~'
  // CHECK-NEXT:       BinaryOperator {{.*}} '-'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:           IntegerLiteral {{.*}} 3
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
  // CHECK-NEXT: }
}

// No inverse
void original_value8(unsigned i, unsigned *p, unsigned arr[1]) {
  // Updated UEQ: { }, Updated G: { i }
  i = i + i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Updated UEQ: { }, Updated G: { i }
  i = 2 * i;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   BinaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Updated UEQ: { }, Update G: { i }
  i += *p;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '+='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     UnaryOperator {{.*}} '*'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }

  // Updated UEQ: { }, Updated G: { i }
  i -= arr[0];
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '-='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     ArraySubscriptExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
  // CHECK-NEXT: Expressions that produce the same value as S:
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: }
}

// Original value from equivalence with another variable
void original_value9(int x, int y) {
  // Updated UEQ: { { y, x } }
  x = y;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of x in x * 2: y
  // Updated UEQ: { { y * 2, x } }
  x *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
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
}

// The left-hand side variable is the original value
void original_value10(int x) {
  // Original value of x in x: x
  // Updated UEQ: { }
  x = x;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }

  // Original value of x in (int)x: x
  // Updated UEQ: { }
  x = (int)x;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:   CStyleCastExpr {{.*}} 'int' <NoOp>
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// CallExpr: using the left-hand side of an assignment as a call argument
void original_value11(array_ptr<int> a : count(1), array_ptr<int> b : count(1)) {
  // Updated UEQ: { { b, a } }
  a = b;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  // Original value of a in g1(a): b
  // Updated UEQ: { { g1(a), a } }
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
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: BoundsValueExpr {{.*}} '_Array_ptr<int>'
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
  // Updated UEQ: { { i, len } }
  int len = i;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  if (flag) {
    // Current UEQ: { { i, len } }
    // Original value of len in j: i
    // Updated UEQ: { { j, len } }
    len = j;
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  }

  // Current UEQ: { }
  // Original value of len in len * 2: null
  // Updated UEQ: { }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// If statement: assignment that does not affect equality sets
void control_flow2(int flag, int i, int j) {
  // Updated UEQ: { { i, len } }
  int len = i;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  if (flag) {
    // Current UEQ: { { i, len } }
    // Updated UEQ: { { i, len }, { 42, j } }
    j = 42;
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT:   IntegerLiteral {{.*}} 42
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
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

  // Current UEQ: { { i, len } }
  // Original value of len in len * 2: i
  // Updated UEQ: { { i * 2, len } }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
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
  // Updated UEQ: { { i, len } }
  int len = i;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
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
    // Current UEQ: { { i, len } }
    // Original value for len in j: i
    // Updated UEQ: { { j, len } }
    len = j;
    // Expected output of len = k from the "else" block
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'k'
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'k'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  } else {
    // Current UEQ: { { i, len } }
    // Original value for len in k: i
    // Updated UEQ: { { k, len } }
    len = k;
    // Expected output of len = j from the "if" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: {
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT: }
    // CHECK-NEXT: }
  }

  // Current UEQ: { }
  // Original value of len in 2 * len: null
  // Updated UEQ: { }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: { }
}

// If/else statements: more complicated intersection of equality sets
void control_flow4(int flag, int x, int y, int z, int w) {
  // Updated UEQ: { { 0, len } }
  int len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
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
    // Current UEQ: { { 0, len } }
    // Updated UEQ: { { 0, len }, { 2, y } }
    y = 2;
    // Expected output of x = 3 from the "else" block
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT:   IntegerLiteral {{.*}} 3
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
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

    // Updated UEQ: { { 0, len }, { 2, y, x } }
    x = y;
    // Expected output of y = x from the "else" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
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

    // Updated UEQ: { { 0, len }, { 2, y, x }, { w, z } }
    z = w;
    // Expected output of w = z from the "else" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'w'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'z'
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
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
    // Current UEQ: { { 0, len } }
    // Updated UEQ: { { 0, len }, { 3, x } }
    x = 3;
    // Expected output of y = 2 from the "if" block
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:   IntegerLiteral {{.*}} 2
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
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

    // Updated UEQ: { { 0, len }, { 3, x, y } }
    y = x;
    // Expected output of x = y from the "if" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'x'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
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

    // Updated UEQ: { { 0, len }, { 3, x, y }, { z, w } }
    w = z;
    // Expected output of z = w from the "if" block
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'z'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'w'
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
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

  // Current UEQ: { { 0, len }, { y, x }, { w, z } }
  // Original value of len in len * 2: null
  // Updated UEQ: { { y, x }, { w, z } }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
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
  // Updated UEQ: { { 0, len } }
  int len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} len
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: {
  // CHECK-NEXT: IntegerLiteral {{.*}} 0
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }
  // CHECK-NEXT: }

  if (1) {
    // Current UEQ: { { 0, len } }
    // Updated UEQ: { { i, len } }
    len = i;
    // CHECK: Statement S:
    // CHECK:      BinaryOperator {{.*}} '='
    // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: Sets of equivalent expressions after checking S:
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

  // Current UEQ: { { i, len } }
  // Original value of len in len * 2: i
  // Updated UEQ: { { i * 2, len } }
  len *= 2;
  // CHECK: Statement S:
  // CHECK-NEXT: CompoundAssignOperator {{.*}} '*='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Sets of equivalent expressions after checking S:
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
