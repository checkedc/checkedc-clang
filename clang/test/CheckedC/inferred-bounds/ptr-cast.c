// Tests of inferred bounds for casts of _Array_ptr values to _Ptr values.
//
// The tests have the general form:
// 1. Some C code.
// 2. A description of the inferred bounds for that C code:
//  a. The expression
//  b. The inferred bounds.
// The description uses AST dumps.  
//
// This test ignores dumps of inferred bounds for other expressions,
// even though they are in the output.
//
// This line is for the clang test infrastructure:
// RUN: %clang_cc1 -fcheckedc-extension -verify -fdump-inferred-bounds %s | FileCheck %s
// expected-no-diagnostics

//-------------------------------------------------------------------------//
// Test taking the address of a variable and casting it to a ptr           //
//-------------------------------------------------------------------------//

void f1(void) {
  int x;
  int y;
  _Ptr<int> p = &x;

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<int>' <BitCast>
// CHECK: |-Inferred SubExpr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: |   |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' prefix '&'
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue Var {{0x[0-9a-f]+}} 'x' 'int'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' '+'
// CHECK: |     |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' prefix '&'
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue Var {{0x[0-9a-f]+}} 'x' 'int'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' prefix '&'
// CHECK:   `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue Var {{0x[0-9a-f]+}} 'x' 'int'

  p = &y;

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<int>' <BitCast>
// CHECK: |-Inferred SubExpr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: |   |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' prefix '&'
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue Var {{0x[0-9a-f]+}} 'y' 'int'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' '+'
// CHECK: |     |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' prefix '&'
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue Var {{0x[0-9a-f]+}} 'y' 'int'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' prefix '&'
// CHECK:   `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue Var {{0x[0-9a-f]+}} 'y' 'int'

}

//--------------------------------------------------------------------------//
// Test taking the address of a dereference or subscript operation and      //
// casting it to a ptr.                                                     //
//--------------------------------------------------------------------------//

void f2(_Array_ptr<int> p : count(5)) {
 // TODO: this produces a typing error unexpectedly.  &(*p) is typed as
 // an Array_ptr<int>.
 //  _Ptr<int> x = &(*p);    /
  _Ptr<int> x = &p[2];

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<int>' <BitCast>
// CHECK: |-Inferred SubExpr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 5
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' prefix '&'
// CHECK:   `-ArraySubscriptExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue
// CHECK:     |-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK:     | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK:     `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 2

}

//--------------------------------------------------------------------------//
// Test taking the address of a member and casting it to a ptr.             //
//--------------------------------------------------------------------------//

struct S {
  int f;
};

void f3(void) {
   struct S v;
   v.f = 0;
   _Ptr<int> p = &v.f;

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<int>' <BitCast>
// CHECK: |-Inferred SubExpr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: |   |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' prefix '&'
// CHECK: |   | `-MemberExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   |   `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue Var {{0x[0-9a-f]+}} 'v' 'struct S':'struct S'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' '+'
// CHECK: |     |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' prefix '&'
// CHECK: |     | `-MemberExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |     |   `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue Var {{0x[0-9a-f]+}} 'v' 'struct S':'struct S'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' prefix '&'
// CHECK:   `-MemberExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue Var {{0x[0-9a-f]+}} 'v' 'struct S':'struct S'

}

//--------------------------------------------------------------------------//
// Test taking the address of a member of an element of 1-dimensional array //
// and casting it to a ptr.                                                 //
//--------------------------------------------------------------------------//
void f4(struct S b _Checked[9]) {
  _Ptr<int> p = &((*b).f);

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<int>' <BitCast>
// CHECK: |-Inferred SubExpr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: |   |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' prefix '&'
// CHECK: |   | `-MemberExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   |   `-ParenExpr {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue
// CHECK: |   |     `-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue prefix '*'
// CHECK: |   |       `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   |         `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' '+'
// CHECK: |     |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' prefix '&'
// CHECK: |     | `-MemberExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |     |   `-ParenExpr {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue
// CHECK: |     |     `-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue prefix '*'
// CHECK: |     |       `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     |         `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' prefix '&'
// CHECK:   `-ParenExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK:       `-ParenExpr {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue
// CHECK:         `-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue prefix '*'
// CHECK:           `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:             `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
}

//--------------------------------------------------------------------------//
// Test taking the address of a member of an element of multim-dimensional  //
// array and casting it to a ptr.                                           //
//--------------------------------------------------------------------------//
void f5(struct S arr _Checked[][12] : count(len), int i, int j, int len) {
  _Ptr<int> p = 0;
  p = &(arr[i][j].f);

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<int>' <BitCast>
// CHECK: |-Inferred SubExpr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: |   |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' prefix '&'
// CHECK: |   | `-MemberExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   |   `-ArraySubscriptExpr {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue
// CHECK: |   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |   |     | `-ArraySubscriptExpr {{0x[0-9a-f]+}} {{.*}} 'struct S _Checked[12]' lvalue
// CHECK: |   |     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' <LValueToRValue>
// CHECK: |   |     |   | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>'
// CHECK: |   |     |   `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: |   |     |     `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: |   |     `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: |   |       `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' '+'
// CHECK: |     |-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' prefix '&'
// CHECK: |     | `-MemberExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |     |   `-ArraySubscriptExpr {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue
// CHECK: |     |     |-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |     |     | `-ArraySubscriptExpr {{0x[0-9a-f]+}} {{.*}} 'struct S _Checked[12]' lvalue
// CHECK: |     |     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' <LValueToRValue>
// CHECK: |     |     |   | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>'
// CHECK: |     |     |   `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: |     |     |     `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: |     |     `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: |     |       `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' prefix '&'
// CHECK:   `-ParenExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK:       `-ArraySubscriptExpr {{0x[0-9a-f]+}} {{.*}} 'struct S':'struct S' lvalue
// CHECK:         |-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:         | `-ArraySubscriptExpr {{0x[0-9a-f]+}} {{.*}} 'struct S _Checked[12]' lvalue
// CHECK:         |   |-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' <LValueToRValue>
// CHECK:         |   | `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>'
// CHECK:         |   `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK:         |     `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK:         `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK:           `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'

}
