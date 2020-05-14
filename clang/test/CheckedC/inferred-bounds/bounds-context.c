// Tests for updating observed bounds during bounds inference and checking.
// This file tests updating the context mapping variables to their bounds
// after checking expressions during bounds analysis.
//
// RUN: %clang_cc1 -Wno-unused-value -fdump-checking-state -verify %s | FileCheck %s

#include <stdchecked.h>

extern array_ptr<int> getArr(void) : count(4);
extern array_ptr<int> getArray(array_ptr<int> arr : count(len), int len, int size) : count(size);
extern array_ptr<int> getArrayWithRange(array_ptr<int> arr) : bounds(arr, arr + 1);

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
  // CHECK-NEXT:     CompoundLiteralExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:       InitListExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 5
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
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
  int b checked[] : count(size) = (int checked[]){ 0 };
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} b
  // CHECK-NEXT:     CountBoundsExpr
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'size'
  // CHECK-NEXT:     CompoundLiteralExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:       InitListExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 5
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
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
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'size'
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
  // CHECK-NEXT:     CompoundLiteralExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:       InitListExpr {{.*}} 'int _Checked[1]'
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
    // CHECK-NEXT:     CompoundLiteralExpr {{.*}} 'int _Checked[1]'
    // CHECK-NEXT:       InitListExpr {{.*}} 'int _Checked[1]'
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
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
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
    // CHECK-NEXT:     CompoundLiteralExpr {{.*}} 'int _Checked[1]'
    // CHECK-NEXT:       InitListExpr {{.*}} 'int _Checked[1]'
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
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
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
    // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
    // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
    // CHECK-NEXT:   BinaryOperator {{.*}} '+'
    // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
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
  // CHECK-NEXT:     CompoundLiteralExpr {{.*}} 'int _Checked[1]'
  // CHECK-NEXT:       InitListExpr {{.*}} 'int _Checked[1]'
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
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
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
void assign1(array_ptr<int> arr : count(1)) {
  // Observed bounds context before assignment: { arr => bounds(arr, arr + 1) }
  // Original value of arr: arr - 2
  // Observed bounds context after assignment:  { arr => bounds(arr - 2, (arr - 2) + 1) }
  arr = arr + 2; // expected-warning {{cannot prove declared bounds for arr are valid after assignment}} \
                 // expected-note {{(expanded) declared bounds are 'bounds(arr, arr + 1)'}} \
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
  // Observed bounds context before assignment: { a => bounds(a, a + len - 1), b => bounds(b, b + len) }
  // Original value of len: len + 3
  // Observed bounds context after assignment : { a => bounds(a, a + ((len + 3) - 1)), b => bounds(b, b + (len + 3)) }
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
  // Observed bounds context before assignment: { a => bounds(unknown), b => bounds(b, b + 1) }
  // Observed bounds context after assignment:  { a => bounds(unknown), b => bounds(b, b + 1) }
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
  // Observed bounds context before assignment: { a => bounds(a, a + len) }
  // Original value of a: a - 1, original value of len: len + 1
  // Observed bounds context after assignment:  { a => bounds(a - 1, (a - 1) + (len + 1)) }
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

  // Observed bounds context before assignment: { a => bounds(a, a + len) }
  // Original value of len: size
  // Observed bounds context after assignment:  { a => bounds(a, a + size) }
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
  // Observed bounds context before assignment: { a => bounds(a, a + len) }
  // Original value of len: null
  // Observed bounds context after assignment:  { a => bounds(unknown) }
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

// Assignment to a variable that is used in the bounds of the RHS of the assignment
void assign7(array_ptr<int> a : bounds(a, a + 1), array_ptr<int> b : bounds(a, a + 1), array_ptr<int> c : bounds(a, a + 1)) {
  // Observed bounds context before assignemnt: { a => bounds(a, a + 1), b => bounds(a, a + 1), c => bounds(a + 1) }
  // Original value of a: null
  // Observed bounds context after assignment:  { a => bounds(unknown), b => bounds(unknown), c => bounds(unknown) }
  a = b; // expected-error {{expression has unknown bounds}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} c
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }

  // Observed bounds context before assignemnt: { a => bounds(a, a + 1), b => bounds(a, a + 1), c => bounds(a, a + 1) }
  // Original value of a: b
  // Observed bounds context after assignment:  { a => bounds(b, b + 1), b => bounds(b, b + 1), c => bounds(b, b + 1) }
  a = c; // expected-warning {{cannot prove declared bounds for a are valid after assignment}} \
         // expected-note {{(expanded) declared bounds are 'bounds(a, a + 1)'}} \
         // expected-note {{(expanded) inferred bounds are 'bounds(b, b + 1)'}}
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} c
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
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
  // Observed bounds context before declaration: { a => bounds(a, a + 1), arr => bounds(arr, arr + 0) }
  // Initializer bounds for a: bounds(a, a + 1)
  // Observed bounds context after declaration:  { a => bounds(a, a + 1), arr => bounds(a, a + 1) }
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Observed bounds context before declaration: { a => bounds(a, a + 1), arr => bounds(arr, arr + 0), buf => bounds(buf, buf + 2) }
  // Initializer bounds for "abc": bounds(temp("abc"), temp("abc") + 3)
  // Observed bounds context after declaration:  { a => bounds(a, a + 1), arr => bounds(arr, arr + 0), buf => bounds(temp("abc"), temp("abc") + 3) }
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} buf
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     BoundsValueExpr {{.*}} 'char [4]'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       BoundsValueExpr {{.*}} 'char [4]'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: }

  // Observed bounds context before declaration: { a => bounds(a, a + 1), arr => bounds(arr, arr + 0), buf => bounds(buf, buf + 2) }
  // Initializer bounds for getArr(): bounds(temp(getArr()), temp(getArr()) + 4)
  // Observed bounds context after declaration:  { a => bounds(a, a + 1), arr => bounds(arr, arr + 0), buf => bounds(buf, buf + 2), c => bounds(temp(getArr()), temp(getArr()) + 4) }
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} buf
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'buf'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'buf'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} c
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }
}

// Non-scalar-typed variable declarations (e.g. arrays) do not set the observed bounds to the initializer bounds
void source_bounds2(void) {
  // Observed bounds context before declaration: { arr => bounds(arr, arr + 1) }
  // Initializer bounds for (int checked[]){ 0, 1, 2 }: bounds(unknown)
  // Observed bounds context after declaration:  { arr => bounds(arr, arr + 1) }
  int arr checked[] : count(1) = (int checked[]){ 0, 1, 2 };
  // CHECK: Statement S:
  // CHECK-NEXT: DeclStmt
  // CHECK-NEXT:   VarDecl {{.*}} arr
  // CHECK-NEXT:     CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     CompoundLiteralExpr {{.*}} 'int _Checked[3]'
  // CHECK-NEXT:       InitListExpr {{.*}} 'int _Checked[3]'
  // CHECK-NEXT:         IntegerLiteral {{.*}} 0
  // CHECK-NEXT:         IntegerLiteral {{.*}} 1
  // CHECK-NEXT:         IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Observed bounds context before declaration: { arr => bounds(arr, arr + 1), buf => bounds(buf, buf + 0) }
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'arr'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} buf
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
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
  // Observed bounds context before assignment: { large => bounds(large, large + 1), small => bounds(small, small + 0) }
  // Source bounds for large: bounds(large, large + 1)
  // Observed bounds context after assignment:  { large => bounds(large, large + 1), small => bounds(large, large + 1) }
  small = large;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} large
  // CHECK-NEXT:  CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:    IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} small
  // CHECK-NEXT:  CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:    IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: }

  // Observed bounds context before assignment: { large => bounds(large, large + 1), small => bounds(small, small + 0) }
  // Source bounds for NullToPointer(0): bounds(any)
  // Observed bounds context after assignment:  { large = bounds(any), small => bounds(small, small + 0) }
  large = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'large'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <NullToPointer>
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Observed bounds context after checking S:
  // CHECK-NEXT: {
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} large
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Any
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} small
  // CHECK-NEXT:  CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:    IntegerLiteral {{.*}} 0
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'small'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} arr
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }
}

////////////////////////////////////////////////////////////////////////////////
// Multiple assignments within one expression that can affect bounds checking //
////////////////////////////////////////////////////////////////////////////////

// Multiple assignments that may result in assignment-related warnings or errors
void multiple_assign1(array_ptr<int> a : count(len), array_ptr<int> b : count(len), unsigned len) {
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'b'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
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
  a++, b = a; // expected-warning {{cannot prove declared bounds for b are valid after assignment}} \
              // expected-note {{(expanded) declared bounds are 'bounds(b, b + len)'}} \
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
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
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
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: }

  // Target bounds of a at assignment a = a: bounds(a, a + len)
  // Observed bounds of a at assignment a = a: bounds(a + 1, a + 1 + len)
  // Observed bounds context after assignments: { a => bounds(a + 1, a + 1 + len), b => bounds(b, b + len) }
  a--, a = a; // expected-warning {{cannot prove declared bounds for a are valid after assignment}} \
              // expected-note {{(expanded) declared bounds are 'bounds(a, a + len)'}} \
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
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
  len = 0, a = b; // expected-error {{expression has unknown bounds, right-hand side of assignment expected to have bounds because the left-hand side has bounds}}
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: }
}

// Multiple assignments that may result in memory access-related errors
void multiple_assign2(array_ptr<int> a : count(len), array_ptr<int> b : bounds(a, a + len), array_ptr<int> c : count(2), unsigned len) {
  // Observed bounds of a at memory access a[len]: bounds(a, a + (len - 1))
  // Observed bounds context after statement: { a => bounds(a, a + (len - 1)), b => bounds(a, a + (len - 1)), c => bounds(c, c + 2) }
  len++, a[len];
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
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} c
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }

  // Observed bounds of c at memory access c[2]: bounds(c - 1, (c - 1) + 2)
  // Observed bounds context after statement: { a => bounds(a, a + len), b => bounds(b, b + len), c => bounds(c - 1, (c - 1) + 2) }
  c++, c[2];
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} ','
  // CHECK-NEXT:   UnaryOperator {{.*}} postfix '++'
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     ArraySubscriptExpr
  // CHECK-NEXT:       Bounds Normal
  // CHECK-NEXT:         RangeBoundsExpr
  // CHECK-NEXT:           BinaryOperator {{.*}} '-'
  // CHECK-NEXT:             ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:               DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:             IntegerLiteral {{.*}} 1
  // CHECK-NEXT:           BinaryOperator {{.*}} '+'
  // CHECK-NEXT:             BinaryOperator {{.*}} '-'
  // CHECK-NEXT:               ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:                 DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:               IntegerLiteral {{.*}} 1
  // CHECK-NEXT:             IntegerLiteral {{.*}} 2
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 2
  // CHECK: Observed bounds context after checking S:
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} c
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BinaryOperator {{.*}} '-'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BinaryOperator {{.*}} '-'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:       IntegerLiteral {{.*}} 1
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }

  // Observed bounds of a at memory access a[0]: bounds(unknown)
  // Observed bounds context after statement: { a => bounds(unknown), b => bounds(unknown), c => bounds(c, c + 2) }
  len = 0, a[0]; // expected-error {{expression has unknown bounds}}
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} c
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }

  // Observed bounds of b at memory access *b: bounds(unknown)
  // Observed bounds context after statement: { a => bounds(unknown), b => bounds(unknown), c => bounds(c, c + 2) }
  a = b, *b; // expected-error 2 {{expression has unknown bounds}}
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   RangeBoundsExpr
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:     BinaryOperator {{.*}} '+'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'a'
  // CHECK-NEXT:       ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:         DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: NullaryBoundsExpr {{.*}} Unknown
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} c
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: }
}

// Single-assignment statements do not result in memory access errors
void multiple_assign3(array_ptr<int> a : count(len), int len) {
  // Observed bounds context before assignment: { a => bounds(a, a + len) }
  // Original value of len: null
  // Observed bounds context after assignment:  { a => bounds(unknown) }
  len = 0;
  // CHECK: Statement S:
  // CHECK-NEXT: BinaryOperator {{.*}} '='
  // CHECK-NEXT:   DeclRefExpr {{.*}} 'len'
  // CHECK-NEXT:   IntegerLiteral {{.*}} 0
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
}

/////////////////////////////////////////
// Nested assignments and declarations //
/////////////////////////////////////////

// Bounds checking accounts for equality information from nested assignments to variables, recorded in State.UEQ
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 1
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:     DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-NEXT:       DeclRefExpr {{.*}} 'c'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} c
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
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

// Pointer deferences are not included in nested assignment information in State.UEQ
void nested_assign2(nt_array_ptr<int> a : count(0), nt_array_ptr<int> b : count(0), ptr<nt_array_ptr<int>> p) {
  // Equality between b and *p is temporarily recorded in order to check the assignment b = *p.
  // Equality between b and *p is not recorded in State.UEQ, so it is not recorded when checking
  // the assignment a = (b = *p).
  // Observed bounds context after all assignments: { a => bounds(*p, *p + 0), b => bounds(*p, *p + 0) }
  a = (b = *p); // expected-warning {{cannot prove declared bounds for a are valid after assignment}} \
                // expected-note {{(expanded) declared bounds are 'bounds(a, a + 0)'}} \
                // expected-note {{(expanded) inferred bounds are 'bounds(*p, *p + 0)'}}
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 0
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

// Bounds checking accounts for equality information from nested initializer and variable assignment, recorded in State.UEQ
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
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: VarDecl {{.*}} a
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 3
  // CHECK: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: Variable:
  // CHECK-NEXT: ParmVarDecl {{.*}} b
  // CHECK-NEXT:   CountBoundsExpr {{.*}} Element
  // CHECK-NEXT:     IntegerLiteral {{.*}} 2
  // CHECK-NEXT: Bounds:
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT:   BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:   BinaryOperator {{.*}} '+'
  // CHECK-NEXT:     BoundsValueExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT:     IntegerLiteral {{.*}} 4
  // CHECK-NEXT: }
}
