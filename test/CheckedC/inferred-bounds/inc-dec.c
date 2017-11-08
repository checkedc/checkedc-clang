// Tests of inferred bounds for unary increment and decrement operators. The
// goal is to check that the bounds are being inferred correctly.
//
// The tests have the general form:
// 1. Some C code.
// 2. A description of the inferred bounds for that C code:
//  a. The expression
//  b. The inferred bounds.
// The description uses AST dumps.
//
// This line is for the clang test infrastructure:
// RUN: %clang_cc1 -fcheckedc-extension -verify -fdump-inferred-bounds %s | FileCheck %s
// expected-no-diagnostics

//-------------------------------------------------------------------------//
// Test post-increment/decrement and pre-increment/decrement of an lvalue  //
// produced by a pointer dereference.                                      //
//-------------------------------------------------------------------------//

void f1(_Array_ptr<int> a : bounds(a, a + 5)) {
  ++*a;

// CHECK: UnaryOperator {{0x[0-9a-f]+}} 'int' prefix '++'
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'

  --*a;

// CHECK: UnaryOperator {{0x[0-9a-f]+}} 'int' prefix '--'
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'

  (*a)++;

// CHECK: UnaryOperator {{0x[0-9a-f]+}} 'int' postfix '++'
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'

  (*a)--;

// CHECK: UnaryOperator {{0x[0-9a-f]+}} 'int' postfix '--'
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'

}

//-------------------------------------------------------------------------//
// Test post-increment/decrement and pre-increment/decrement of an lvalue  //
// produced by array subscripting a local defined array                    //
//-------------------------------------------------------------------------//

int f2(void) {
  int arr _Checked[6] = { 0, 1, 2, 3, 4 };
  int sum = 0;
  sum = ++arr[0];

// CHECK: UnaryOperator {{0x[0-9a-f]+}} 'int' prefix '++'
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 0

  sum = --arr[0];

// CHECK: UnaryOperator {{0x[0-9a-f]+}} 'int' prefix '--'
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 0

  sum = arr[1]++;

// CHECK: UnaryOperator {{0x[0-9a-f]+}} 'int' postfix '++'
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

  sum = arr[1]--;

// CHECK: UnaryOperator {{0x[0-9a-f]+}} 'int' postfix '--'
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK:`-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

  return sum;
}

struct S {
  int f;
};

//-------------------------------------------------------------------------//
// Test post-increment/decrement and pre-increment/decrement of an lvalue  //
// produced by member reference.                                           //
//-------------------------------------------------------------------------//
void f3(_Array_ptr<struct S> p : count(2)) {
  p->f++;

//
// There are only bounds checks on the bases of the member expressions. The
// lvalues produced by the member accesses are guaranteed to be in-bounds.
//

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'

  ++p->f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'

  p->f--;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'

  --p->f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'

  (*p).f++;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'

  ++(*p).f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2

// CHECK:     `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'

  (*p).f--;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'

  --(*p).f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<struct S>'

}
