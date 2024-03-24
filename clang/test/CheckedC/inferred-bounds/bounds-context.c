// Tests for updating the observed bounds context during bounds inference and checking.
// This file tests updating the context mapping variables to their bounds
// after checking expressions during bounds analysis.
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state -verify %s | FileCheck %s

#include <stdchecked.h>

extern array_ptr<int> getArr(void) : count(4);
extern array_ptr<int> getArray(array_ptr<int> arr : count(len), int len, int size) : count(size);
extern array_ptr<int> getArrayWithRange(array_ptr<int> arr) : bounds(arr, arr + 1);
extern void testArgBounds(array_ptr<int> a : count(len), int len);
extern void testNtArray(nt_array_ptr<char> p : count(0), int i);

////////////////////////////////////////////////
// No assignments to variables used in bounds //
////////////////////////////////////////////////

// Parameter and local variables with declared count bounds
void declared1(array_ptr<int> arr : count(len), int len, int size) {
  // Observed bounds context: { a => bounds(a, a + 5), arr => bounds(arr, arr + len) }
  int a checked[] : count(5) = (int checked[]){ 0 };
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} a
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       IntegerLiteral {{.*}} 5
  // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:       CompoundLiteralExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:         InitListExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' 'int _Checked[1]'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 5
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' '_Array_ptr<int>'
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

  // Observed bounds context: { a => bounds(a, a + 5), b => bounds(b, b + size), arr => bounds(arr, arr + len) }
  int b checked[] : count(size) = (int checked[]){ 0 };
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} b
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:       CompoundLiteralExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:         InitListExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' 'int _Checked[1]'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 5
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' 'int _Checked[1]'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' '_Array_ptr<int>'
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
}

// If statement, redeclared variable
void declared2(int flag, int x, int y) {
  // Observed bounds context: { a => bounds(a, a + x) }
  int a checked[] : count(x) = (int checked[]){ 0 };
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} a
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:       CompoundLiteralExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:         InitListExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' 'int _Checked[1]'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }

  if (flag) {
    // Observed bounds context: { a => bounds(a, a + x), a => bounds(a, a + y) }
    int a checked[] : count(y) = (int checked[]){ 0 };
    // CHECK: Statement S:
    // CHECK:      DeclStmt
    // CHECK-NEXT:   VarDecl {{.*}} a
    // CHECK-NEXT:     CountBoundsExpr
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'int _Checked[1]'
    // CHECK-NEXT:       CompoundLiteralExpr {{.*}} 'int _Checked[1]'
    // CHECK-NEXT:         InitListExpr {{.*}} 'int _Checked[1]'
    // CHECK-NEXT:           IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'a' 'int _Checked[1]'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
    // CHECK: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'a' 'int _Checked[1]'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: }

    // Observed bounds context: { a => bounds(a, a + x), a => bounds(a, a + y), b => bounds(b, b + y) }
    int b checked[] : count(y) = (int checked []){ 0 };
    // CHECK: Statement S:
    // CHECK:      DeclStmt
    // CHECK-NEXT:   VarDecl {{.*}} b
    // CHECK-NEXT:     CountBoundsExpr
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'int _Checked[1]'
    // CHECK-NEXT:       CompoundLiteralExpr {{.*}} 'int _Checked[1]'
    // CHECK-NEXT:         InitListExpr {{.*}} 'int _Checked[1]'
    // CHECK-NEXT:           IntegerLiteral {{.*}} 0
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'a' 'int _Checked[1]'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
    // CHECK: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'a' 'int _Checked[1]'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
    // CHECK: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'b' 'int _Checked[1]'
    // CHECK: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'y'
    // CHECK-NEXT: }
  }

  // Observed bounds context: { a => bounds(a, a + x), c => bounds(c, c + x) }
  int c checked[] : count(x) = (int checked []){ 0 };
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} c
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:       CompoundLiteralExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:         InitListExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' 'int _Checked[1]'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'c' 'int _Checked[1]'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'x'
  // CHECK-NEXT: }
}

/////////////////////////////////////////////
// Assignments to variables used in bounds //
/////////////////////////////////////////////

// Assignment to a variable used in its own bounds
void assign1(array_ptr<int> arr : count(1)) { // expected-note {{(expanded) declared bounds are 'bounds(arr, arr + 1)'}}
  // Observed bounds context before assignment: { arr => bounds(arr, arr + 1) }
  // Original value of arr: arr - 2
  // Observed bounds context after assignment:  { arr => bounds(arr - 2, (arr - 2) + 1) }
  arr = arr + 2; // expected-warning {{cannot prove declared bounds for 'arr' are valid after assignment}} \
                 // expected-note {{(expanded) inferred bounds are 'bounds(arr - 2, arr - 2 + 1)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' '_Array_ptr<int>'
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
void assign2(
  array_ptr<int> a : count(len - 1), // expected-note {{(expanded) declared bounds are 'bounds(a, a + len - 1)'}}
  char b nt_checked[0] : count(len),
  unsigned len
) {
  // Observed bounds context before assignment: { a => bounds(a, a + len - 1), b => bounds(b, b + len) }
  // Original value of len: len + 3
  // Observed bounds context after assignment : { a => bounds(a, a + ((len + 3) - 1)), b => bounds(b, b + (len + 3)) }
  len = len - 3; // expected-warning {{cannot prove declared bounds for 'a' are valid after assignment}} \
                 // expected-note {{(expanded) inferred bounds are 'bounds(a, a + len + 3 - 1)'}} \
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
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
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
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Nt_array_ptr<char>'
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
  // Observed bounds context before assignment: { a => bounds(unknown), b => bounds(b, b + 1) }
  // Observed bounds context after assignment:  { a => bounds(unknown), b => bounds(b, b + 1) }
  len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Nt_array_ptr<char>'
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
void assign4(array_ptr<int> a : count(len), unsigned len) { // expected-note {{(expanded) declared bounds are 'bounds(a, a + len)'}}
  // Observed bounds context before assignment: { a => bounds(a, a + len) }
  // Original value of a: a - 1, original value of len: len + 1
  // Observed bounds context after assignment:  { a => bounds(a - 1, (a - 1) + (len + 1)) }
  ++a, len--; // expected-warning {{cannot prove declared bounds for 'a' are valid after decrement}} \
              // expected-note {{(expanded) inferred bounds are 'bounds(a - 1, a - 1 + len + 1U)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
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
void assign5(array_ptr<int> a : count(len), int len, int size) { // expected-note {{(expanded) declared bounds are 'bounds(a, a + len)'}}
  // Observed bounds context before assignment: { a => bounds(a, a + len) }
  // Observed bounds context after assignment:  { a => bounds(a, a + len) }
  size = len;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
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

  // Observed bounds context before assignment: { a => bounds(a, a + len) }
  // Original value of len: size
  // Observed bounds context after assignment:  { a => bounds(a, a + size) }
  len = len * 2; // expected-warning {{cannot prove declared bounds for 'a' are valid after assignment}} \
                 // expected-note {{(expanded) inferred bounds are 'bounds(a, a + size)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   BinaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
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
void assign6(array_ptr<int> a : count(len), int len) { // expected-note {{(expanded) declared bounds are 'bounds(a, a + len)'}}
  // Observed bounds context before assignment: { a => bounds(a, a + len) }
  // Original value of len: null
  // Observed bounds context after assignment:  { a => bounds(unknown) }
  len = len * 2; // expected-error {{inferred bounds for 'a' are unknown after assignment}} \
                 // expected-note {{lost the value of the expression 'len' which is used in the (expanded) inferred bounds 'bounds(a, a + len)' of 'a'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   BinaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}

// Assignment to a variable that is used in the bounds of the RHS of the assignment
void assign7(
  array_ptr<int> a : bounds(a, a + 1), // expected-note 2 {{(expanded) declared bounds are 'bounds(a, a + 1)'}}
  array_ptr<int> b : bounds(a, a + 1), // expected-note 2 {{(expanded) declared bounds are 'bounds(a, a + 1)'}}
  array_ptr<int> c : bounds(a, a + 1) // expected-note 2 {{(expanded) declared bounds are 'bounds(a, a + 1)'}}
) {
  // Observed bounds context before assignemnt: { a => bounds(a, a + 1), b => bounds(a, a + 1), c => bounds(a + 1) }
  // Original value of a: null
  // Observed bounds context after assignment:  { a => bounds(unknown), b => bounds(unknown), c => bounds(unknown) }
  a = b; // expected-error {{inferred bounds for 'a' are unknown after assignment}} \
         // expected-note {{lost the value of the expression 'a' which is used in the (expanded) inferred bounds 'bounds(a, a + 1)' of 'a'}} \
         // expected-error {{inferred bounds for 'b' are unknown after assignment}} \
         // expected-note {{lost the value of the expression 'a' which is used in the (expanded) inferred bounds 'bounds(a, a + 1)' of 'b'}} \
         // expected-error {{inferred bounds for 'c' are unknown after assignment}} \
         // expected-note {{lost the value of the expression 'a' which is used in the (expanded) inferred bounds 'bounds(a, a + 1)' of 'c'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'c' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }

  // Observed bounds context before assignment: { a => bounds(a, a + 1), b => bounds(a, a + 1), c => bounds(a, a + 1) }
  // Original value of a: b
  // Observed bounds context after assignment:  { a => bounds(b, b + 1), b => bounds(b, b + 1), c => bounds(b, b + 1) }
  a = c; // expected-error {{it is not possible to prove that the inferred bounds of 'a' imply the declared bounds of 'a' after assignment}} \
         // expected-error {{it is not possible to prove that the inferred bounds of 'b' imply the declared bounds of 'b' after assignment}} \
         // expected-error {{it is not possible to prove that the inferred bounds of 'c' imply the declared bounds of 'c' after assignment}} \
         // expected-note 3 {{the declared bounds use the variable 'a' and there is no relational information involving 'a' and any of the expressions used by the inferred bounds}} \
         // expected-note 3 {{the inferred bounds use the variable 'b' and there is no relational information involving 'b' and any of the expressions used by the declared bounds}} \
         // expected-note 3 {{(expanded) inferred bounds are 'bounds(b, b + 1)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'c' '_Array_ptr<int>'
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

////////////////////////////////////////////////////////////////////
// Setting the observed bounds of a variable to the source bounds //
////////////////////////////////////////////////////////////////////

// Scalar-typed variable declarations (array_ptr, nt_array_ptr) set the observed bounds to the initializer bounds
void source_bounds1(array_ptr<int> a: count(1)) {
  // Initializer bounds for a: bounds(a, a + 1)
  // Observed bounds context after declaration:  { arr => bounds(a, a + 1), a => bounds(a, a + 1) }
  array_ptr<int> arr : count(0) = a;
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} arr
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' '_Array_ptr<int>'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Initializer bounds for "abc": bounds(temp("abc"), temp("abc") + 3)
  // Observed bounds context after declaration:  { arr => bounds(arr, arr + 0), buf => bounds(temp("abc"), temp("abc") + 3), a => bounds(a, a + 1) }
  nt_array_ptr<char> buf : count(2) = "abc";
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} buf
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       CHKCBindTemporaryExpr {{.*}} 'char [4]'
  // CHECK-NEXT:         StringLiteral {{.*}} "abc"
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' '_Array_ptr<int>'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'buf' '_Nt_array_ptr<char>'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     BoundsValueExpr {{.*}} 'char [4]'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       BoundsValueExpr {{.*}} 'char [4]'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Initializer bounds for getArr(): bounds(temp(getArr()), temp(getArr()) + 4)
  // Observed bounds context after declaration:  { arr => bounds(arr, arr + 0), buf => bounds(buf, buf + 2), c => bounds(temp(getArr()), temp(getArr()) + 4), a => bounds(a, a + 1) }
  array_ptr<int> c : count(3) = getArr();
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} c
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:       CallExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'getArr'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' '_Array_ptr<int>'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'buf' '_Nt_array_ptr<char>'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'buf'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'buf'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'c' '_Array_ptr<int>'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// Non-scalar-typed variable declarations (e.g. arrays) do not set the observed bounds to the initializer bounds
void source_bounds2(void) {
  // Initializer bounds for (int checked[]){ 0, 1, 2 }: bounds(unknown)
  // Observed bounds context after declaration:  { arr => bounds(arr, arr + 1) }
  int arr checked[] : count(1) = (int checked[]){ 0, 1, 2 };
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} arr
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'int _Checked[3]'
  // CHECK-NEXT:       CompoundLiteralExpr {{.*}} 'int _Checked[3]'
  // CHECK-NEXT:         InitListExpr {{.*}} 'int _Checked[3]'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT:           IntegerLiteral {{.*}} 1
  // CHECK-NEXT:           IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' 'int _Checked[3]'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Initializer bounds for "abcde": bounds(unknown)
  // Observed bounds context after declaration:  { arr => bounds(arr, arr + 1), buf => bounds(buf, buf + 0) }
  char buf nt_checked[] : count(0) = "abcde";
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} buf
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT:     StringLiteral {{.*}} "abcde"
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' 'int _Checked[3]'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'buf' 'char _Nt_checked[6]'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'buf'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'buf'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: }
}

// Assignments to variables set the observed bounds to the source bounds
// where the LHS variable does not appear on the RHS of the assignment
void source_bounds3(array_ptr<int> small : count(0), array_ptr<int> large : count(1)) {
  // Observed bounds context before assignment: { small => bounds(small, small + 0), large => bounds(large, large + 1) }
  // Source bounds for large: bounds(large, large + 1)
  // Observed bounds context after assignment:  { small => bounds(large, large + 1), large => bounds(large, large + 1) }
  small = large;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'small' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'large' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Observed bounds context before assignment: { small => bounds(small, small + 0), large => bounds(large, large + 1),  }
  // Source bounds for NullToPointer(0): bounds(any)
  // Observed bounds context after assignment:  { small => bounds(small, small + 0), large = bounds(any) }
  large = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'small' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'large' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
  // CHECK-NEXT: }
}

// Assignments to variables set the observed bounds to the source bounds
// where the LHS variable appears on the RHS of the assignment as part of a temporary binding
void source_bounds4(array_ptr<int> arr : count(1)) {
  // Observed bounds context before assignment: { arr => bounds(arr, arr + 1) }
  // Source bounds for the dynamic bounds cast: bounds(temp(arr), temp(arr) + 2)
  // Observed bounds context after assignment:  { arr => bounds(temp(arr), temp(arr) + 2) }
  arr = _Dynamic_bounds_cast<array_ptr<int>>(arr, count(2));
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BoundsCastExpr {{.*}} <DynamicPtrBounds>
  // CHECK:          CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   CStyleCastExpr {{.*}} '_Array_ptr<int>' <BitCast>
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     CStyleCastExpr {{.*}} '_Array_ptr<int>' <BitCast>
  // CHECK-NEXT:       BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }

  // Observed bounds context before assignment: { arr => bounds(arr, arr + 1) }
  // Source bounds for the assume bounds cast: bounds(temp(arr), temp(arr) + 3)
  // Observed bounds context after assignment:  { arr => bounds(temp(arr), temp(arr) + 3) }
  arr = _Assume_bounds_cast<array_ptr<int>>(arr, count(3));
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BoundsCastExpr {{.*}} <AssumePtrBounds>
  // CHECK:          CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   CStyleCastExpr {{.*}} '_Array_ptr<int>' <BitCast>
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     CStyleCastExpr {{.*}} '_Array_ptr<int>' <BitCast>
  // CHECK-NEXT:       BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }

  // Observed bounds context before assignment: { arr => bounds(arr, arr + 1) }
  // Source bounds for the call: bounds(temp(getArray(arr, 1, 4)), temp(getArray(arr, 1, 4)) + 4)
  // Observed bounds context after assignment:  { arr => bounds(temp(getArray(arr, 1, 4)), temp(getArray(arr, 1, 4)) + 4) }
  arr = getArray(arr, 1, 4);
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     CallExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'getArray'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:       IntegerLiteral {{.*}} 4
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }
}

// Assignments to variables set the observed bounds to the source bounds
// where the source is an array-typed compound literal
void source_bounds5(array_ptr<int> arr_array_literal : count(2)) {
  // Observed bounds context: { arr_array_literal => bounds(value(temp((int checked[2]){ 0, 1 })), value(temp((int checked[2]){ 0, 1 })) + 2) }
  arr_array_literal = (int checked[2]){0, 1};
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'arr_array_literal'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     CHKCBindTemporaryExpr {{.*}} 'int _Checked[2]' lvalue
  // CHECK-NEXT:       CompoundLiteralExpr {{.*}} 'int _Checked[2]' lvalue
  // CHECK-NEXT:         InitListExpr {{.*}} 'int _Checked[2]'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT:           IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr_array_literal' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     BoundsValueExpr {{.*}} 'int _Checked[2]' lvalue
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       BoundsValueExpr {{.*}} 'int _Checked[2]' lvalue
  // CHECK-NEXT:     IntegerLiteral {{.*}} 'int' 2
  // CHECK-NEXT: }
}

struct a {
  int f;
};

// Assignments to variables set the observed bounds to the source bounds
// where the source is a struct-typed compound literal
void source_bounds6() {
  // Observed bounds context: { arr_struct_literal => bounds(&value(temp((struct a){ 0 })), &value(temp((struct a){ 0 })) + 1) }
  array_ptr<struct a> arr_struct_literal : count(1) = &(struct a){0};
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} a
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <BitCast>
  // CHECK-NEXT:     UnaryOperator {{.*}} prefix '&'
  // CHECK-NEXT:       CHKCBindTemporaryExpr {{.*}} 'struct a':'struct a' lvalue
  // CHECK-NEXT:         CompoundLiteralExpr {{.*}} 'struct a':'struct a' lvalue
  // CHECK-NEXT:           InitListExpr {{.*}} 'struct a':'struct a'
  // CHECK-NEXT:             IntegerLiteral {{.*}} 'int' 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'arr_struct_literal' '_Array_ptr<struct a>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   UnaryOperator {{.*}} prefix '&'
  // CHECK-NEXT:     BoundsValueExpr {{.*}} 'struct a':'struct a' lvalue
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     UnaryOperator {{.*}} prefix '&'
  // CHECK-NEXT:       BoundsValueExpr {{.*}} 'struct a':'struct a' lvalue
  // CHECK-NEXT:     IntegerLiteral {{.*}} 'int' 1
  // CHECK-NEXT: }
}

////////////////////////////////////////////////////////////////////////////////
// Multiple assignments within one expression that can affect bounds checking //
////////////////////////////////////////////////////////////////////////////////

// Multiple assignments that may result in assignment-related warnings or errors
void multiple_assign1(
  array_ptr<int> a : count(len), // expected-note 3 {{(expanded) declared bounds are 'bounds(a, a + len)'}}
  array_ptr<int> b : count(len), // expected-note 2 {{(expanded) declared bounds are 'bounds(b, b + len)'}}
  unsigned len
) {
  // Target bounds of a at assignment a = b: bounds(a, a + len)
  // Observed bounds of b at assignment a = b: bounds(b, b + len)
  // Observed bounds context after assignments: { a => bounds(b, b + len), b => bounds(b, b + len) }
  a++, a = b;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }

  // Target bounds of b at assignment b = a: bounds(b, b + len)
  // Observed bounds of a at assignment b = a: bounds(a - 1, a - 1 + len)
  // Observed bounds context after assignments: { a => bounds(a - 1, a - 1 + len), b => bounds(a - 1, a - 1 + len) }
  a++, b = a; // expected-warning {{cannot prove declared bounds for 'a' are valid after increment}} \
              // expected-note {{(expanded) inferred bounds are 'bounds(a - 1, a - 1 + len)'}} \
              // expected-warning {{cannot prove declared bounds for 'b' are valid after assignment}} \
              // expected-note {{(expanded) inferred bounds are 'bounds(a - 1, a - 1 + len)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
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
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
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
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }

  // Target bounds of a at assignment a = a: bounds(a, a + len)
  // Observed bounds of a at assignment a = a: bounds(a + 1, a + 1 + len)
  // Observed bounds context after assignments: { a => bounds(a + 1, a + 1 + len), b => bounds(b, b + len) }
  a--, a = a; // expected-warning {{cannot prove declared bounds for 'a' are valid after assignment}} \
              // expected-note {{(expanded) inferred bounds are 'bounds(a + 1, a + 1 + len)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }

  // Target bounds of a at assignment a = b: bounds(a, a + len)
  // Observed bounds of b at assignment a = b: bounds(unknown)
  // Observed bounds context after assignments: { a => bounds(unknown), b => bounds(unknown) }
  len = 0, a = b; // expected-error {{inferred bounds for 'a' are unknown after assignment}} \
                  // expected-note {{lost the value of the expression 'len' which is used in the (expanded) inferred bounds 'bounds(a, a + len)' of 'a'}} \
                  // expected-error {{inferred bounds for 'b' are unknown after assignment}} \
                  // expected-note {{lost the value of the expression 'len' which is used in the (expanded) inferred bounds 'bounds(b, b + len)' of 'b'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}

// Multiple assignments involving variable-sized bounds that may result in memory access-related errors
void multiple_assign2(
  array_ptr<int> a : count(len), // expected-note 3 {{(expanded) declared bounds are 'bounds(a, a + len)'}}
  array_ptr<int> b : bounds(a, a + len), // expected-note 3 {{(expanded) declared bounds are 'bounds(a, a + len)'}}
  unsigned len
) {
  // Observed bounds of a at memory access a[len]: bounds(a, a + (len - 1))
  // Observed bounds context after statement: { a => bounds(a, a + (len - 1)), b => bounds(a, a + (len - 1)) }
  len++, a[len]; // expected-warning {{cannot prove declared bounds for 'a' are valid after increment}} \
                 // expected-warning {{cannot prove declared bounds for 'b' are valid after increment}} \
                 // expected-note 2 {{(expanded) inferred bounds are 'bounds(a, a + len - 1U)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     ArraySubscriptExpr
  // CHECK-NEXT:       Bounds Normal
  // CHECK-NEXT:         RangeBoundsExpr
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT:           BinaryOperator {{.*}} '+'
  // CHECK-NEXT:             ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:               DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:             BinaryOperator {{.*}} '-'
  // CHECK-NEXT:               ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:                 DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:               IntegerLiteral {{.*}} 1
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Observed bounds of a at memory access a[0]: bounds(unknown)
  // Observed bounds context after statement: { a => bounds(unknown), b => bounds(unknown) }
  len = 0, a[0]; // expected-error {{expression has unknown bounds}} \
                 // expected-error {{inferred bounds for 'a' are unknown after assignment}} \
                 // expected-note {{lost the value of the expression 'len' which is used in the (expanded) inferred bounds 'bounds(a, a + len)' of 'a'}} \
                 // expected-error {{inferred bounds for 'b' are unknown after assignment}} \
                 // expected-note {{lost the value of the expression 'len' which is used in the (expanded) inferred bounds 'bounds(a, a + len)' of 'b'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue
  // CHECK-NEXT:     ArraySubscriptExpr
  // CHECK-NEXT:       Bounds Normal
  // CHECK-NEXT:         NullaryBoundsExpr {{.*}} Invalid
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }

  // Observed bounds of b at memory access *b: bounds(unknown)
  // Observed bounds context after statement: { a => bounds(unknown), b => bounds(unknown) }
  a = b, *b; // expected-error {{expression has unknown bounds}} \
             // expected-error {{inferred bounds for 'a' are unknown after assignment}} \
             // expected-note {{lost the value of the expression 'a' which is used in the (expanded) inferred bounds 'bounds(a, a + len)' of 'a'}} \
             // expected-error {{inferred bounds for 'b' are unknown after assignment}} \
             // expected-note {{lost the value of the expression 'a' which is used in the (expanded) inferred bounds 'bounds(a, a + len)' of 'b'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     UnaryOperator {{.*}} '*'
  // CHECK-NEXT:       Bounds Normal
  // CHECK-NEXT:         NullaryBoundsExpr {{.*}} Invalid
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}

// Multiple assignments involving constant-sized bounds that may result in memory access-related errors
void multiple_assign3(
  array_ptr<int> a : count(2),
  array_ptr<int> b : count(1)
) {
  // Observed bounds of a at memory access a[1]: bounds(b, b + 1)
  // Observed bounds context after statement: { a => bounds(any), b => bounds(b, b + 1) }
  a = b, a[1], a = 0; // expected-error {{out-of-bounds memory access}} \
                      // expected-note {{accesses memory at or above the upper bound}} \
                      // expected-note {{(expanded) inferred bounds are 'bounds(b, b + 1)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} ','
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       ArraySubscriptExpr
  // CHECK-NEXT:         Bounds Normal
  // CHECK-NEXT:           RangeBoundsExpr
  // CHECK-NEXT:             ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:               DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:             BinaryOperator {{.*}} '+'
  // CHECK-NEXT:               ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:                 DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:               IntegerLiteral {{.*}} 1
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:       IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
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

// Single-assignment statements do not result in memory access errors
void multiple_assign4(array_ptr<int> a : count(len), int len) { // expected-note {{(expanded) declared bounds are 'bounds(a, a + len)'}}
  // Observed bounds context before assignment: { a => bounds(a, a + len) }
  // Original value of len: null
  // Observed bounds context after assignment:  { a => bounds(unknown) }
  len = 0; // expected-error {{inferred bounds for 'a' are unknown after assignment}} \
           // expected-note {{lost the value of the expression 'len' which is used in the (expanded) inferred bounds 'bounds(a, a + len)' of 'a'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }

  // Observed bounds of a at memory access *a: bounds(a, a + len)
  // Observed bounds context after statement: { a => bounds(a, a + len) }
  *a;
  // CHECK: Statement S:
  // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:   UnaryOperator {{.*}} '*'
  // CHECK-NEXT:     Bounds Normal
  // CHECK-NEXT:       RangeBoundsExpr
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:         BinaryOperator {{.*}} '+'
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
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
}

/////////////////////////////////////////
// Nested assignments and declarations //
/////////////////////////////////////////

// Bounds checking accounts for equality information from nested assignments to variables, recorded in State.EquivExprs
void nested_assign1(nt_array_ptr<int> a : count(1), nt_array_ptr<const int> b : count(2), nt_array_ptr<volatile int> c : count(3)) {
  // Observed bounds context after all assignments: { a => bounds(c, c + 3), b => bounds(c, c + 3), c => bounds(c, c + 3) }
  a = (b = c); // expected-warning {{assigning to '_Nt_array_ptr<const int>' from '_Nt_array_ptr<volatile int>' discards qualifiers}} \
               // expected-warning {{assigning to '_Nt_array_ptr<int>' from '_Nt_array_ptr<const int>' discards qualifiers}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} '_Nt_array_ptr<int>' <NoOp>
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} '_Nt_array_ptr<const int>' <NoOp
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} '_Nt_array_ptr<volatile int>' <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Nt_array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Nt_array_ptr<const int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'c' '_Nt_array_ptr<volatile int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }
}

// Pointer deferences are included in temporary expression equality while validating bounds
void nested_assign2(
  nt_array_ptr<int> a : count(0),
  nt_array_ptr<int> b : count(0),
  ptr<nt_array_ptr<int>> p
) {
  // Observed bounds context after all assignments: { a => bounds(*p, *p + 0), b => bounds(*p, *p + 0) }
  a = (b = *p);
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         UnaryOperator {{.*}} '*'
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Nt_array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     UnaryOperator {{.*}} '*'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       UnaryOperator {{.*}} '*'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Nt_array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     UnaryOperator {{.*}} '*'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       UnaryOperator {{.*}} '*'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: }
}

// Bounds checking accounts for equality information from nested initializer and
// variable assignment, recorded in State.EquivExprs
void nested_assign3(array_ptr<int> b : count(2)) {
  // Observed bounds context before checking initializer and assignment: { a => bounds(a, a + 3), b => bounds(b, b + 2) }
  // Observed bounds context after checking initializer and assignment:  { a => bounds(temp(getArr()), temp(getArr()) + 4), b => bounds(temp(getArr()), temp(getArr()) + 4) }
  array_ptr<int> a : count(3) = (b = getArr());
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} a
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 3
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:         CHKCBindTemporaryExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:           CallExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:             ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:               DeclRefExpr {{.*}} 'getArr'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }
}

// Bounds checking accounts for equality information in State.EquivExprs
// when checking call argument bounds
void nested_assign4(array_ptr<int> a : count(2), array_ptr<int> b : count(3)) {
  // Observed bounds context before statement: { a => bounds(a, a + 2), b => bounds(b, b + 3) }
  // Expected bounds of a at call: bounds(a, a + 3)
  // Observed bounds of a at call: bounds(b, b + 3)
  // Observed bounds context after statement:  { a => bounds(b, b + 3), b => bounds(b, b + 3) }
  a = b, testArgBounds(a, 3);
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   CallExpr {{.*}} 'void'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <FunctionToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'testArgBounds'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }
}

/////////////////////////////////////////////////
// Updating the result bounds of an assignment //
/////////////////////////////////////////////////

// Updated result bounds of a nested assignment with a binary operator
void update_result_bounds1(
  array_ptr<int> a : bounds(b, b + 1), // expected-note {{(expanded) declared bounds are 'bounds(b, b + 1)'}}
  array_ptr<int> b : count(1) // expected-note {{(expanded) declared bounds are 'bounds(b, b + 1)'}}
) {
  // Observed bounds context before assignments: { a => bounds(b, b + 1), b => bounds(b, b + 1) }
  // Bounds of b = b + 1: bounds(b - 1, (b - 1) + 1)
  // Observed bounds context after assignments: { a => bounds(b - 1, (b - 1 + 1)), b => bounds(b - 1, (b - 1) + 1) }
  a = (b = b + 1); // expected-warning {{cannot prove declared bounds for 'b' are valid after assignment}} \
                   // expected-warning {{cannot prove declared bounds for 'a' are valid after assignment}} \
                   // expected-note 2 {{(expanded) inferred bounds are 'bounds(b - 1, b - 1 + 1)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       BinaryOperator {{.*}} '+'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// Updated result bounds of a nested assignment with a compound operator
void update_result_bounds2(
  array_ptr<int> a : bounds(b, b + 1), // expected-note {{(expanded) declared bounds are 'bounds(b, b + 1)'}}
  array_ptr<int> b : count(1) // expected-note {{(expanded) declared bounds are 'bounds(b, b + 1)'}}
) {
  // Observed bounds context before assignments: { a => bounds(b, b + 1), b => bounds(b, b + 1) }
  // Bounds of b += 1: bounds(b - 1, (b - 1) + 1)
  // Observed bounds context after assignments: { a => bounds(b - 1, (b - 1 + 1)), b => bounds(b - 1, (b - 1) + 1) }
  a = (b += 1); // expected-warning {{cannot prove declared bounds for 'b' are valid after assignment}} \
                // expected-warning {{cannot prove declared bounds for 'a' are valid after assignment}} \
                // expected-note 2 {{(expanded) inferred bounds are 'bounds(b - 1, b - 1 + 1)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     CompoundAssignOperator {{.*}} '+='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// Updated result bounds of a nested assignment with a post-increment operator
void update_result_bounds3(
  array_ptr<int> a : bounds(b, b + 1), // expected-note {{(expanded) declared bounds are 'bounds(b, b + 1)'}}
  array_ptr<int> b : count(1) // expected-note {{(expanded) declared bounds are 'bounds(b, b + 1)'}}
) {
  // Observed bounds context before assignments: { a => bounds(b, b + 1), b => bounds(b, b + 1) }
  // Bounds of b++: bounds(b - 1, (b - 1) + 1)
  // Observed bounds context after assignments: { a => bounds(b - 1, (b - 1 + 1)), b => bounds(b - 1, (b - 1) + 1) }
  a = b++; // expected-warning {{cannot prove declared bounds for 'a' are valid after assignment}} \
           // expected-warning {{cannot prove declared bounds for 'b' are valid after increment}} \
           // expected-note 2 {{(expanded) inferred bounds are 'bounds(b - 1, b - 1 + 1)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

///////////////////////////////////////////////////////////////////////////
// Checking the updated source bounds of an increment/decrement operator //
///////////////////////////////////////////////////////////////////////////

// Pre-increment operator: bounds warning
void inc_dec_bounds1(nt_array_ptr<char> a) { // expected-note {{(expanded) declared bounds are 'bounds(a, a + 0)'}}
  // Observed bounds context before increment: { a => bounds(a, a + 0) }
  // Observed bounds context after increment:  { a => bounds(a - 1, (a - 1) + 0) }
  ++a; // expected-warning {{cannot prove declared bounds for 'a' are valid after increment}} \
       // expected-note {{(expanded) inferred bounds are 'bounds(a - 1, a - 1 + 0)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Nt_array_ptr<char>'
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
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: }
}

// Post-increment operator: bounds error
void inc_dec_bounds2(nt_array_ptr<int> a : bounds(a, a)) { // expected-note {{(expanded) declared bounds are 'bounds(a, a)'}}
  // Observed bounds context before increment: { a => bounds(a, a) }
  // Observed bounds context after increment:  { a => bounds(a - 1, a - 1) }
  a++; // expected-error {{declared bounds for 'a' are invalid after increment}} \
       // expected-note {{source bounds are an empty range}} \
       // expected-note {{destination upper bound is above source upper bound}} \
       // expected-note {{(expanded) inferred bounds are 'bounds(a - 1, a - 1)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Nt_array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// Pre-decrement operator: bounds warning
void inc_dec_bounds3(array_ptr<float> a : count(2)) { // expected-note {{(expanded) declared bounds are 'bounds(a, a + 2)'}}
  // Observed bounds context before decrement: { a => bounds(a, a + 2) }
  // Observed bounds context after decrement:  { a => bounds(a + 1, (a + 1) + 2) }
  --a; // expected-warning {{cannot prove declared bounds for 'a' are valid after decrement}} \
       // expected-note {{(expanded) inferred bounds are 'bounds(a + 1, a + 1 + 2)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<float>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }
}

// Post-decrement operator: bounds error
void inc_dec_bounds4(array_ptr<int> a : bounds(a, a)) { // expected-note {{(expanded) declared bounds are 'bounds(a, a)'}}
  // Observed bounds context before decrement: { a => bounds(a, a) }
  // Observed bounds context after decrement:  { a => bounds(a + 1, (a + 1)) }
  a--; // expected-error {{declared bounds for 'a' are invalid after decrement}} \
       // expected-note {{(expanded) inferred bounds are 'bounds(a + 1, a + 1)'}} \
       // expected-note {{source bounds are an empty range}} \
       // expected-note {{destination lower bound is below source lower bound}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }
}

// Increment/decrement operators on dereference expressions or array subscripts
// of type _Nt_array_ptr<T> are bounds checked
// Increment/decrement operators on variables without declared bounds are not
// bounds checked
void inc_dec_bounds5(nt_array_ptr<int> *p, array_ptr<int> a) {
  // Observed bounds context after increment:  { *p => bounds(*p - 1, (*p - 1) + 0) }
  ++*p; // expected-warning {{cannot prove declared bounds for '*p' are valid after increment}} \
        // expected-note {{(expanded) declared bounds are 'bounds(*p, *p + 0)'}} \
        // expected-note {{(expanded) inferred bounds are 'bounds(*p - 1, *p - 1 + 0)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '++'
  // CHECK-NEXT:   UnaryOperator {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT:   UnaryOperator {{.*}} '_Nt_array_ptr<int>' {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} '_Nt_array_ptr<int> *' <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<int> *'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       UnaryOperator {{.*}} '*'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}}  <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         UnaryOperator {{.*}} '*'
  // CHECK-NEXT:           ImplicitCastExpr {{.*}}  <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: }

  // Observed bounds context after increment:  { *p => bounds(p[0] - 1, (p[0] - 1) + 0) }
  // Note: *p is the representative expression of the AbstractSet containing *p and p[0]
  p[0]++; // expected-warning {{cannot prove declared bounds for '*p' are valid after increment}} \
          // expected-note {{(expanded) declared bounds are 'bounds(*p, *p + 0)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(p[0] - 1, p[0] - 1 + 0)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:   ArraySubscriptExpr {{.*}} '_Nt_array_ptr<int>'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT:   UnaryOperator {{.*}} '_Nt_array_ptr<int>' {{.*}} '*'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} '_Nt_array_ptr<int> *' <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<int> *'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       ArraySubscriptExpr {{.*}} '_Nt_array_ptr<int>'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}}  <LValueToRValue>
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 0
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         ArraySubscriptExpr {{.*}} '_Nt_array_ptr<int>'
  // CHECK-NEXT:           ImplicitCastExpr {{.*}}  <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'p'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: }

  // Observed bounds context after increment:  { }
  a--;
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: { }
}

// Increment/decrement operators on variables used in a _Nt_array_ptr<char>
void inc_dec_bounds6(nt_array_ptr<char> p : count(i), unsigned int i) { // expected-note 2 {{(expanded) declared bounds are 'bounds(p, p + i)'}}
  // Observed bounds context after increment: { p => bounds(p, p + i - 1) }
  i++; // expected-warning {{cannot prove declared bounds for 'p' are valid after increment}} \
       // expected-note {{(expanded) inferred bounds are 'bounds(p, p + i - 1U)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i' 'unsigned int'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'i' 'unsigned int'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT: }

  // Observed bounds context after decrement: { p => bounds(p + 1, p + 1 + i) }
  --p; // expected-warning {{cannot prove declared bounds for 'p' are valid after decrement}} \
       // expected-note {{(expanded) inferred bounds are 'bounds(p + 1, p + 1 + i)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} prefix '--'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 'int' 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 'int' 1
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'i' 'unsigned int'
  // CHECK-NEXT: }
}

// Increment/decrement operators on variables used in a _Nt_array_ptr<char>
void inc_dec_bounds7(nt_array_ptr<char> p : count((3 * i)), unsigned int i) { // expected-note {{(expanded) declared bounds are 'bounds(p, p + (3 * i))'}}
  // Observed bounds context after increment: { p => bounds(p, p + 3 * (i - 1)) }
  i++; // expected-warning {{cannot prove declared bounds for 'p' are valid after increment}} \
       // expected-note {{(expanded) inferred bounds are 'bounds(p, p + (3 * i - 1U))'}}
  // CHECK: Statement S:
  // CHECK-NEXT: UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i' 'unsigned int'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
  // CHECK-NEXT:     ParenExpr
  // CHECK-NEXT:       BinaryOperator {{.*}} '*'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} 'unsigned int' <IntegralCast>
  // CHECK-NEXT:           IntegerLiteral {{.*}} 3
  // CHECK-NEXT:         BinaryOperator {{.*}} '-'
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'i' 'unsigned int'
  // CHECK-NEXT:           IntegerLiteral {{.*}} 'unsigned int' 1
  // CHECK-NEXT: }
}

///////////////////////////////////////////////////////////////////////////
// Expressions that contain multiple assignments can kill widened bounds //
///////////////////////////////////////////////////////////////////////////

// Widened bounds killed by a statement with multiple assignments
void killed_widened_bounds1(
  nt_array_ptr<int> p : count(i), // expected-note {{(expanded) declared bounds are 'bounds(p, p + i)'}}
  int i,
  int other
) {
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
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<int>'
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
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<int>'
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
    // Observed bounds context: { p => bounds(unknown) }
    i++, --other; // expected-error {{inferred bounds for 'p' are unknown after increment}} \
                  // expected-note {{lost the value of the expression 'i' which is used in the (expanded) inferred bounds 'bounds(p, p + i + 1)' of 'p'}}
    // CHECK: Statement S:
    // CHECK-NEXT: BinaryOperator {{.*}} ','
    // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT:   UnaryOperator {{.*}} prefix '--'
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'other'
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<int>'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
    // CHECK-NEXT: }
  }
}

// Widened bounds killed by a statement with multiple assignments
void killed_widened_bounds2(nt_array_ptr<char> p : count(0), int other) {
  if (*p) {
    // Observed bounds context: { p => bounds(p, p + 0) }
    // CHECK: Statement S:
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   UnaryOperator {{.*}} '*'
    // CHECK:          ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK:      Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
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
    // Observed bounds context: { p => bounds(p, p + 1) }
    p[1];
    // CHECK: Statement S:
    // CHECK-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:   ArraySubscriptExpr
    // CHECK-NEXT:     Bounds Null-terminated read
    // CHECK-NEXT:       RangeBoundsExpr
    // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:           DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:         BinaryOperator {{.*}} '+'
    // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:             DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:           IntegerLiteral {{.*}} 1
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     IntegerLiteral {{.*}} 1
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     IntegerLiteral {{.*}} 1
    // CHECK-NEXT: }

    // This statement kills the widened bounds of p since it modifies p
    // Observed bounds context: { p = bounds(any) }
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
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
    // CHECK-NEXT: }
  }
}

// Widened bounds of multiple variables killed by a statement with multiple assignments
void killed_widened_bounds3(
  nt_array_ptr<char> p : count(i), // expected-note {{(expanded) declared bounds are 'bounds(p, p + i)'}}
  int i,
  nt_array_ptr<int> q : count(1) // expected-note {{(expanded) declared bounds are 'bounds(q, q + 1)'}}
) {
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
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'q' '_Nt_array_ptr<int>'
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
      // CHECK-NEXT: LValue Expression:
      // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
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
      // CHECK-NEXT: LValue Expression:
      // CHECK-NEXT: DeclRefExpr {{.*}} 'q' '_Nt_array_ptr<int>'
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
      // CHECK-NEXT: LValue Expression:
      // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
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
      // CHECK-NEXT: LValue Expression:
      // CHECK-NEXT: DeclRefExpr {{.*}} 'q' '_Nt_array_ptr<int>'
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
      // Observed bounds context: { p => bounds(unknown), q => bounds(q - 1, q - 1 + 1 + 1) }
      i = 0, q++; // expected-error {{inferred bounds for 'p' are unknown after assignment}} \
                  // expected-note {{lost the value of the expression 'i' which is used in the (expanded) inferred bounds 'bounds(p, p + i + 1)' of 'p'}} \
                  // expected-warning {{cannot prove declared bounds for 'q' are valid after increment}} \
                  // expected-note {{(expanded) inferred bounds are 'bounds(q - 1, q - 1 + 1 + 1)'}}
      // CHECK: Statement S:
      // CHECK-NEXT: BinaryOperator {{.*}} ','
      // CHECK-NEXT:   BinaryOperator {{.*}} '='
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT:     IntegerLiteral {{.*}} 0
      // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
      // CHECK: Observed bounds context after checking S:
      // CHECK-NEXT: {
      // CHECK-NEXT: LValue Expression:
      // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
      // CHECK-NEXT: LValue Expression:
      // CHECK-NEXT: DeclRefExpr {{.*}} 'q' '_Nt_array_ptr<int>'
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: RangeBoundsExpr
      // CHECK-NEXT:   BinaryOperator {{.*}} '-'
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT:   BinaryOperator {{.*}} '+'
      // CHECK-NEXT:     BinaryOperator {{.*}} '+'
      // CHECK-NEXT:       BinaryOperator {{.*}} '-'
      // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:           DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:         IntegerLiteral {{.*}} 1
      // CHECK-NEXT:       IntegerLiteral {{.*}} 1
      // CHECK-NEXT:     IntegerLiteral {{.*}} 1
      // CHECK-NEXT: }
    }
  }
}

////////////////////////////////////////////////////////////////////////
// The bounds context is updated after checking conditional operators //
////////////////////////////////////////////////////////////////////////

// The observed bounds contexts in each arm are equivalent
void conditionals1(array_ptr<int> a : count(1),
                   array_ptr<int> b : count(2), // expected-note {{(expanded) declared bounds are 'bounds(b, b + 2)'}}
                   int flag) {
  // No assignments in either arm
  // Observed bounds context: { a => bounds(a, a + 1), b => bounds(b, b + 2) }
  flag ? a : b;
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'flag'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }

  // Observed bounds context: { a => bounds(any), b => bounds(any) }
  1 ? (a = 0, b = 0) : (b = 0, a = 0);
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} ','
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} ','
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT:       BinaryOperator {{.*}} '='
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:         ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:           IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
  // CHECK-NEXT: }

  // Observed bounds context: { a => bounds(a, a + 1), b => bounds(a, a + 1) }
  1 ? (b = a) : (b = a); // expected-error {{declared bounds for 'b' are invalid after statement}} \
                         // expected-note {{destination bounds are wider than the source bounds}} \
                         // expected-note {{destination upper bound is above source upper bound}} \
                         // expected-note {{(expanded) inferred bounds are 'bounds(a, a + 1)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Observed bounds context: { a => bounds(b, b + 2), b => bounds(b, b + 2) }
  // Since a and b are not updated in the arms of the conditional operator, a and b are known to be equal
  (a = b) ? a : b;
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }
}

// The observed bounds contexts in each arm are not equivalent
void conditional2(array_ptr<int> a : count(1),
                  array_ptr<int> b : count(2), // expected-note {{(expanded) declared bounds are 'bounds(b, b + 2)'}}
                  array_ptr<int> c : count(3), 
                  array_ptr<int> d : count(4)) { // expected-note {{(expanded) declared bounds are 'bounds(d, d + 4)'}}
  // Bounds updated in "true" arm: { a => bounds(b, b + 2) }
  // Bounds updated in "false" arm: { }
  // Observed bounds context: { a => bounds(a, a + 1), b => bounds(b, b + 2), c => bounds(c, c + 3), d => bounds(d, d + 4) }
  1 ? (a = b) : a;
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'c' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'd' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }

  // Bounds written in "true" arm: { b => bounds(a, a + 1) }
  // Bounds written in "false" arm: { d => bounds(c, c + 3) }
  // Observed bounds context: { a => bounds(a, a + 1), b => bounds(b, b + 2), c => bounds(c, c + 3), d => bounds(d, d + 4) }
  1 ? (b = a) : (d = c); // expected-error {{declared bounds for 'b' are invalid after assignment}} \
                         // expected-note {{(expanded) inferred bounds are 'bounds(a, a + 1)'}} \
                         // expected-error {{declared bounds for 'd' are invalid after assignment}} \
                         // expected-note {{(expanded) inferred bounds are 'bounds(c, c + 3)'}} \
                         // expected-note 2 {{destination bounds are wider than the source bounds}} \
                         // expected-note 2 {{destination upper bound is above source upper bound}}
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'c' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'd' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }

  // Bounds written in "true" arm: { a => bounds(b, b + 2) }
  // Bounds written in "false" arm: { a => bounds(c, c + 3) }
  // Observed bounds context: { a => bounds(a, a + 1), b => bounds(b, b + 2), c => bounds(c, c + 3), d => bounds(d, d + 4) }
  (a = d) ? (a = b) : (a = c);
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'a' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'b' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'c' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'd' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'd'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }
}

// Conditional operators with widened bounds
void conditional3(nt_array_ptr<char> p : count(i),
                  unsigned i, 
                  nt_array_ptr<char> q : count(j), // expected-note {{(expanded) declared bounds are 'bounds(q, q + j)'}}
                  unsigned j) {
  if (*(p + i)) {
    // Observed bounds context: { p => bounds(p, p + i - 1 + 1), q => bounds(q, q + j) }
    1 ? i++ : ++i;
    // CHECK: Statement S:
    // CHECK:      ConditionalOperator
    // CHECK-NEXT:   IntegerLiteral {{.*}} 1
    // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT:   UnaryOperator {{.*}} prefix '++'
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT: Observed bounds context after checking S:
    // CHECK-NEXT: {
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     BinaryOperator {{.*}} '+'
    // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:         DeclRefExpr {{.*}} 'p'
    // CHECK-NEXT:       BinaryOperator {{.*}} '-'
    // CHECK-NEXT:         ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:           DeclRefExpr {{.*}} 'i'
    // CHECK-NEXT:         IntegerLiteral {{.*}} 'unsigned int' 1
    // CHECK-NEXT:       IntegerLiteral {{.*}} 'int' 1
    // CHECK-NEXT: LValue Expression:
    // CHECK-NEXT: DeclRefExpr {{.*}} 'q' '_Nt_array_ptr<char>'
    // CHECK-NEXT: Bounds:
    // CHECK-NEXT: RangeBoundsExpr
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'q'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
    // CHECK-NEXT:       DeclRefExpr {{.*}} 'j'
    // CHECK-NEXT: }

    if (*(q + j)) {
      // Bounds written in condition: { q => bounds(q - 2, q - 2 + j) }
      // Bounds written in "true" arm: { q => bounds(any) }
      // Bounds written in "false" arm: { q => bounds(q + 1 - 2, q + 1 - 2 + j + 1) }
      // Observed bounds context: {  p => bounds(p, p + i), q => bounds(q, q + j + 1) }
      (q += 2) ? (q = 0) : q--; // expected-warning {{cannot prove declared bounds for 'q' are valid after decrement}} \
                                // expected-note {{(expanded) inferred bounds are 'bounds(q + 1 - 2, q + 1 - 2 + j + 1)'}}
      // CHECK: Statement S:
      // CHECK:      ConditionalOperator
      // CHECK-NEXT:   ParenExpr
      // CHECK-NEXT:     CompoundAssignOperator {{.*}} '+='
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:       IntegerLiteral {{.*}} 2
      // CHECK-NEXT:   ParenExpr
      // CHECK-NEXT:     BinaryOperator {{.*}} '='
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:       ImplicitCastExpr {{.*}} <NullToPointer>
      // CHECK-NEXT:         IntegerLiteral {{.*}} 0
      // CHECK-NEXT:   UnaryOperator {{.*}} postfix '--'
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT: Observed bounds context after checking S:
      // CHECK-NEXT: {
      // CHECK-NEXT: LValue Expression:
      // CHECK-NEXT: DeclRefExpr {{.*}} 'p' '_Nt_array_ptr<char>'
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: RangeBoundsExpr
      // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'p'
      // CHECK-NEXT:   BinaryOperator {{.*}} '+'
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'p'
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'i'
      // CHECK-NEXT: LValue Expression:
      // CHECK-NEXT: DeclRefExpr {{.*}} 'q' '_Nt_array_ptr<char>'
      // CHECK-NEXT: Bounds:
      // CHECK-NEXT: RangeBoundsExpr
      // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:     DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:   BinaryOperator {{.*}} '+'
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'q'
      // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
      // CHECK-NEXT:       DeclRefExpr {{.*}} 'j'
      // CHECK-NEXT: }
    }
  }
}

// Bounds updated (and valid) in one arm, not updated (and not provably valid) in other arm
void conditional4(array_ptr<int> large : count(3),
                  array_ptr<int> medium : count(2), // expected-note 2 {{(expanded) declared bounds are 'bounds(medium, medium + 2)'}}
                  array_ptr<int> small : count(1)) {
  // Bounds updated and validated in "true" arm: { medium => bounds(large, large + 3) }
  // Bounds not updated, but validated in "false" arm: { medium => bounds(small, small + 1) } - error in false arm
  // Observed bounds context: { large => bounds(large, large + 3) medium => bounds(medium, medium + 2), small => bounds(small, small + 1) }
  medium = small, (1 ? (medium = large) : medium); // expected-error {{declared bounds for 'medium' are invalid after statement}} \
                                                   // expected-note {{(expanded) inferred bounds are 'bounds(small, small + 1)'}} \
                                                   // expected-note {{destination bounds are wider than the source bounds}} \
                                                   // expected-note {{destination upper bound is above source upper bound}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   BinaryOperator {{.*}} '='
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     ConditionalOperator
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:       ParenExpr
  // CHECK-NEXT:         BinaryOperator {{.*}} '='
  // CHECK-NEXT:           DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT:           ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:             DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'large' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'medium' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'small' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Bounds not updated, but validated in "true" arm: { medium => bounds((medium + 1), (medium + 1) + 2) } - warning in true arm
  // Bounds updated and validated in "false" arm: { medium => bounds(any) }
  // Observed bounds context: { large => bounds(large, large + 3) medium => bounds(medium, medium + 2), small => bounds(small, small + 1) }
  medium-- ? medium : (medium = 0); // expected-warning {{cannot prove declared bounds for 'medium' are valid after statement}} \
                                    // expected-note {{(expanded) inferred bounds are 'bounds(medium + 1, medium + 1 + 2)'}} \
  // CHECK: Statement S:
  // CHECK-NEXT: ConditionalOperator
  // CHECK-NEXT:   UnaryOperator {{.*}} postfix '--'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT:   ParenExpr
  // CHECK-NEXT:     BinaryOperator {{.*}} '='
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:         IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'large' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'medium' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'medium'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: LValue Expression:
  // CHECK-NEXT: DeclRefExpr {{.*}} 'small' '_Array_ptr<int>'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }  
}

//////////////////////////////////////////////////////////////////////////
// Invalid variable declarations are not included in the bounds context //
//////////////////////////////////////////////////////////////////////////

// Parameter with an invalid declaration
_Checked void invalid_decl1(int **p : count(i), int i) { // expected-error {{parameter in a checked scope must have a bounds-safe interface type that uses only checked types or parameter/return types with bounds-safe interfaces}} \
                                                         // expected-note {{'int *' is not allowed in a checked scope}}
  // Even though i is used in the declared bounds of p, since p is an invalid
  // declaration, we do not include p in the observed bounds context, so this
  // statement does not invalidate any bounds.
  // Observed bounds context: { }
  i = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: { }
}

// Local variable with an invalid declaration
_Checked void invalid_decl2(int i) {
  // p is an invalid declaration, so we do not include p in the bounds context.
  // Observed bounds context: { }
  _Array_ptr<char *> p : count(i) = 0; // expected-error {{local variable in a checked scope must have a pointer, array or function type that uses only checked types or parameter/return types with bounds-safe interfaces}} \
                                       // expected-note {{'char *' is not allowed in a checked scope}}
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} invalid p
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: { }

  // Even though i is used in the declared bounds of p, since p is an invalid
  // declaration, we do not include p in the observed bounds context, so this
  // statement does not invalidate any bounds.
  // Observed bounds context: { }
  i = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'i'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: { }
}
