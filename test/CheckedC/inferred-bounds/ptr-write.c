// Tests of inferred bounds for writes though pointers.  The goal is to check
// that the bounds are being inferred correctly.
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
// Test assignment through a pointer passed as parameter.                  //
//-------------------------------------------------------------------------//

void f1(_Array_ptr<int> a : bounds(a, a + 5)) {
  *a = 100;

// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int' '='
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: | |-Bounds
// CHECK: | | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: | |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: | |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: | |    |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: | |    `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 100
// CHECK: Target Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Unknown

  a[3] = 101;

// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int' '='
// CHECK: |-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: | |-Bounds
// CHECK: | | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: | |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: | |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: | |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: | |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |  `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 101
// CHECK: Target Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Unknown

}

//-------------------------------------------------------------------------//
// Test assignment to an element of a single-dimensional local array       //
//-------------------------------------------------------------------------//

int f2(void) {
  int arr _Checked[6] = { 0, 1, 2, 3, 4 };
  *arr = 3;

// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int' '='
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: |-Bounds
// CHECK: | | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: | |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: | |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: | |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: | |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: Target Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Unknown

  arr[2] = 4;

// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int' '='
// CHECK: |-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: | |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: | |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: | |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: | |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 4
// CHECK: Target Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Unknown

  return arr[2];
}

//--------------------------------------------------------------------------//
// Test assignment to an element of a single-dimensional array passed as a  //
// parameter                                                                //
//--------------------------------------------------------------------------//

void f3(int b _Checked[7]) {
  *b = 102;

// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int' '='
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: |-Bounds
// CHECK: | | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: | |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: | |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: | |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: | |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: | |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: | |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 102
// CHECK: Target Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Unknown

  b[3] = 103;

// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int' '='
// CHECK: |-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: | |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: | |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: | |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: | |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: | |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: | |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK: | |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: | | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 103
// CHECK: Target Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Unknown

}

//--------------------------------------------------------------------------//
// Test assignment to an element of a multi-dimensional array passed as a   //
// parameter                                                                //
//--------------------------------------------------------------------------//

void f4(int arg _Checked[10][10]) {
   arg[5][5] = 314;
}


// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int' '='
// CHECK: |-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: | |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' <LValueToRValue>
// CHECK: | |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arg' '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>'
// CHECK: | |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' '+'
// CHECK: | |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' <LValueToRValue>
// CHECK: | |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arg' '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>'
// CHECK: | |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 10
// CHECK: | |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | | `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int _Checked[10]' lvalue
// CHECK: | |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' <LValueToRValue>
// CHECK: | |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arg' '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>'
// CHECK: | |   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 314
// CHECK: Target Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Unknown