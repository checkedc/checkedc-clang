// Tests of inferred bounds for reads though pointers.  The goal is to check
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

int f1(_Array_ptr<int> a : bounds(a, a + 5)) {
  int x = *a;

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'

  int y = a[3];

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3

  return x + y;
}

//-------------------------------------------------------------------------//
// Test reading an element of a single-dimensional local array             //
//-------------------------------------------------------------------------//

int f2(void) {
  int arr _Checked[6] = { 0, 1, 2, 3, 4 };
  int x = *arr;

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6

// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'

  int y = arr[2];

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int _Checked[6]'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2

  return x + y;
}

//--------------------------------------------------------------------------//
// Test readomg an element of a single-dimensional array passed as a        //
// parameter                                                                //
//--------------------------------------------------------------------------//

int f3(int b _Checked[7]) {
  int x = *b;

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'int' lvalue prefix '*'
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'

  int y = b[3];

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>':'_Array_ptr<int>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3

   return x + y;
}

//--------------------------------------------------------------------------//
// Test reading an element of a multi-dimensional array passed as a         //
// parameter                                                                //
//--------------------------------------------------------------------------//

int f4(int arr _Checked[][10] : count(len), int i, int j, int len) {
  int x = arr[i++][j];
  return x;
}

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: |-Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>'
// CHECK: |     `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: |       `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'len' 'int'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:   | `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'int _Checked[10]' lvalue
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<int _Checked[10]>':'_Array_ptr<int _Checked[10]>'
// CHECK:   |   `-UnaryOperator {{0x[0-9a-f]+}} 'int' postfix '++'
// CHECK:   |     `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'