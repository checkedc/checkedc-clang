// Tests of inferred bounds for base expressions of member references of
// structure or unions.  The goal is to check that the bounds are being 
// inferred correctly.
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

struct S {
  int f;
};

//-------------------------------------------------------------------------//
// Test reading through a pointer passed as parameter.                     //
//-------------------------------------------------------------------------//

int f1(_Array_ptr<struct S> a : bounds(a, a + 5)) {
  int x = (*a).f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'

  int y = a->f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'

  int z = a[3].f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3

  return x + y + z;
}

//-------------------------------------------------------------------------//
// Test writing through a pointer passed as parameter.                     //
//-------------------------------------------------------------------------//

void f2(_Array_ptr<struct S> a : bounds(a, a + 6)) {
  (*a).f = 1;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK:     |-Bounds
// CHECK:     | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:     |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK:     |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:     |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'

  a->f = 2;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'

  a[3].f = 3;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
}

//-------------------------------------------------------------------------//
// Test taking the address of a field access via a pointer passed as a     //
// parameter.                                                              //
//-------------------------------------------------------------------------//

void f3(_Array_ptr<struct S> a : bounds(a, a + 7)) {
  _Array_ptr<int> p : count(1) = &((*a).f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK:     |-Bounds
// CHECK:     | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:     |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK:     |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:     |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'

  *p = 1;
  p = &(a->f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'

// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:           `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: RHS Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

  *p = 2;
  p = &(a[3].f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3

// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK: `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:           |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK:           | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK:           `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: RHS Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
  *p = 3;
}


//-------------------------------------------------------------------------//
// Test reading, writing and taking the adddress of elements of single     //
// dimensional arrays.  These tests are combined into one function to      //
// avoids uses of uninitialized data.                                      //
//-------------------------------------------------------------------------//

int f10(void) {
  struct S arr _Checked[6];
  for (int i = 0; i < 6; i++) {
    arr[i].f = i;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue Var {{0x[0-9a-f]+}} 'i' 'int'

  }
  return 0;
}

int f10a(void) {
  struct S arr _Checked[6];
  int x = (*arr).f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK:     |-Bounds
// CHECK:     | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:     |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:     |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:     |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:     |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:     |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK:     `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'

  int y = arr->f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'

  int z = arr[2].f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2

  _Array_ptr<int> p : count(1) = &((*arr).f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK:     |-Bounds
// CHECK:     | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:     |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:     |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:     |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:     |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:     |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'

  *p = 10;
  p = &(arr->f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'

// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK:     `-ParenExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK:       `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK:         `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:           `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: RHS Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

  *p = 11;
  p = &(arr[3].f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 6
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3

// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK:     `-ParenExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK:       `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK:         `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:           |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:           | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK:           `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: RHS Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S _Checked[6]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'struct S _Checked[6]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

  *p = 12;
  return x + y + z;
}

//--------------------------------------------------------------------------//
// Test reading an element of a single-dimensional array passed as a        //
// parameter                                                                //
//--------------------------------------------------------------------------//

int f20(struct S b _Checked[7]) {
  int x = (*b).f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK:     |-Bounds
// CHECK:     | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:     |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:     |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:     |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK:     `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'

  int y = b->f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'

  int z = b[3].f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 7
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3

  return x + y + z;
}


//-------------------------------------------------------------------------//
// Test writing to an element of a single-dimensional array passed as a    //
// parameter.                                                              //
//-------------------------------------------------------------------------//

void f21(struct S b _Checked[8]) {
  (*b).f = 1;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK:     |-Bounds
// CHECK:     | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:     |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:     |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:     |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 8
// CHECK:     `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'

  b->f = 2;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 8
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'

  b[3].f = 3;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 8
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3

}

//-------------------------------------------------------------------------//
// Test taking the address of an element of a single-dimensional array     //
// passed as a parameter.                                                  //
//-------------------------------------------------------------------------//

void f22(struct S b _Checked[9]) {
  _Array_ptr<int> p : count(1) = &((*b).f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ParenExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue prefix '*'
// CHECK:     |-Bounds
// CHECK:     | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:     |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:     |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:     |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:     |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 9
// CHECK:     `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'

  *p = 1;
  p = &(b->f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |-Base Expr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 9
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'

// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK:     `-ParenExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK:       `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK:         `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:           `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: RHS Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue ->f {{0x[0-9a-f]+}}
// CHECK: |   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

  *p = 2;
  p = &(b[3].f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 9
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:   -IntegerLiteral {{0x[0-9a-f]+}} 'int' 3

// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK:     `-ParenExpr {{0x[0-9a-f]+}} 'int' lvalue
// CHECK:       `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK:         `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:           |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK:           | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK:           `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<int>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: RHS Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' <LValueToRValue>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>':'_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<struct S>':'_Array_ptr<struct S>'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

  *p = 3;
}

//--------------------------------------------------------------------------//
// Test reading an element of a multi-dimensional array passed as a         //
// parameter                                                                //
//--------------------------------------------------------------------------//

int f30(struct S arr _Checked[][10] : count(len), int i, int j, int len) {
  int x = arr[i++][j].f;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>'
// CHECK:   |     `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   |       `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'len' 'int'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   | `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S _Checked[10]' lvalue
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[10]>':'_Array_ptr<struct S _Checked[10]>'
// CHECK:   |   `-UnaryOperator {{0x[0-9a-f]+}} 'int' postfix '++'
// CHECK:   |     `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'

  return x;
}

void f31(struct S arr _Checked[][11] : count(len), int i, int j, int len) {
  arr[i++][j].f = 314;

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>'
// CHECK:   |     `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   |       `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'len' 'int'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   | `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S _Checked[11]' lvalue
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[11]>':'_Array_ptr<struct S _Checked[11]>'
// CHECK:   |   `-UnaryOperator {{0x[0-9a-f]+}} 'int' postfix '++'
// CHECK:   |     `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
}

void f32(struct S arr _Checked[][12] : count(len), int i, int j, int len) {
  _Ptr<int> p = &(arr[i][j].f);

// CHECK: MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>'
// CHECK:   |     `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   |       `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'len' 'int'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <ArrayToPointerDecay>
// CHECK:   | `-ArraySubscriptExpr {{0x[0-9a-f]+}} 'struct S _Checked[12]' lvalue
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S _Checked[12]>':'_Array_ptr<struct S _Checked[12]>'
// CHECK:   |   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   |     `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'

  *p = 316;
}
