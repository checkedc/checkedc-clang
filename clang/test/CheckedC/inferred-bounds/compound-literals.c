// Tests of inferred bounds for expressions involving compound literals.
// The goal is to check that the bounds are being inferred correctly.
//
// The tests have the general form:
// 1. Some C code.
// 2. A description of the inferred bounds for that C code:
//  a. The expression
//  b. The inferred bounds.
// The description uses AST dumps.
//
// This line is for the clang test infrastructure:
// RUN: %clang_cc1 -fcheckedc-extension -verify -verify-ignore-unexpected=warning -verify-ignore-unexpected=note -fdump-inferred-bounds %s | FileCheck %s --dump-input=always

// expected-no-diagnostics

struct S {
  int i;
  _Ptr<int> p;
  _Nt_array_ptr<char> buf;
};

//-------------------------------------------------------------------------//
// Test array-typed compound literals                                      //
//-------------------------------------------------------------------------//

void f1(_Array_ptr<int> a : count(2), _Array_ptr<int[1]> b : count(1)) {
  // Target LHS bounds: bounds(a, a + 2)
  // Inferred RHS bounds: bounds(temp((int[]){ 0, 1 }), temp((int[]){ 0, 1 }) + 2)
  a = (int[]){ 0, 1 };
  // CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
  // CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
  // CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay> BoundsSafeInterface
  // CHECK: `-CHKCBindTemporaryExpr {{0x[0-9a-f]+}} 'int [2]' lvalue
  // CHECK: `-CompoundLiteralExpr {{0x[0-9a-f]+}} 'int [2]' lvalue
  // CHECK: `-InitListExpr {{0x[0-9a-f]+}} 'int [2]'
  // CHECK: |-IntegerLiteral {{0x[0-9a-f]+}} 'int' 0
  // CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
  // CHECK: Target Bounds:
  // CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
  // CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
  // CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
  // CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
  // CHECK:  |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
  // CHECK:  | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
  // CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
  // CHECK: RHS Bounds:
  // CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
  // CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
  // CHECK: | `-BoundsValueExpr {{0x[0-9a-f]+}} 'int [2]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
  // CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *':'int *' '+'
  // CHECK:  |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
  // CHECK:  | `-BoundsValueExpr {{0x[0-9a-f]+}} 'int [2]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
  // CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2

  // Target LHS bounds: bounds(b, b + 1)
  // Inferred RHS bounds: bounds(temp((int[1]{ 0, 1, 2 })), temp(int[1]{ 0, 1, 2 }) + 1)
  b = &(int[1]){ 0, 1, 2 };
  // CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int [1]>' '='
  // CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int [1]>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int [1]>'
  // CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int [1]>' <BitCast>
  // CHECK:  `-UnaryOperator {{0x[0-9a-f]+}} 'int (*)[1]' prefix '&' cannot overflow
  // CHECK:  `-CHKCBindTemporaryExpr {{0x[0-9a-f]+}} 'int [1]' lvalue
  // CHECK:  `-CompoundLiteralExpr {{0x[0-9a-f]+}} 'int [1]' lvalue
  // CHECK:  `-InitListExpr {{0x[0-9a-f]+}} 'int [1]'
  // CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 0
  // CHECK: Target Bounds:
  // CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
  // CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int [1]>' <LValueToRValue>
  // CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int [1]>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int [1]>'
  // CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int [1]>' '+'
  // CHECK:  |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int [1]>' <LValueToRValue>
  // CHECK:  | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int [1]>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int [1]>'
  // CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
  // CHECK: RHS Bounds:
  // CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
  // CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
  // CHECK: | `-BoundsValueExpr {{0x[0-9a-f]+}} 'int [1]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
  // CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *':'int *' '+'
  // CHECK:  |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
  // CHECK:  | `-BoundsValueExpr {{0x[0-9a-f]+}} 'int [1]' lvalue _BoundTemporary {{0x[0-9a-f]+}}
  // CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
}

//-------------------------------------------------------------------------//
// Test struct-typed compound literals (and variables)                     //
//-------------------------------------------------------------------------//

void f2(_Array_ptr<struct S> arr : count(1), struct S s) {
  // Target LHS bounds: bounds(arr, arr + 1)
  // Inferred RHS bounds: bounds(&s, &s + 1)
  arr = &s;
  // CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '='
  // CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S>'
  // CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <BitCast>
  // CHECK:  `-UnaryOperator {{0x[0-9a-f]+}} 'struct S *' prefix '&' cannot overflow
  // CHECK:  `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue ParmVar {{0x[0-9a-f]+}} 's' 'struct S':'struct S'
  // CHECK: Target Bounds:
  // CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
  // CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
  // CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S>'
  // CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
  // CHECK:  |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
  // CHECK:  | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S>'
  // CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
  // CHECK: RHS Bounds:
  // CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
  // CHECK: |-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' prefix '&' cannot overflow
  // CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue ParmVar {{0x[0-9a-f]+}} 's' 'struct S':'struct S'
  // CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
  // CHECK:  |-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' prefix '&' cannot overflow
  // CHECK:  | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue ParmVar {{0x[0-9a-f]+}} 's' 'struct S':'struct S'
  // CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

  // Target LHS bounds: bounds(arr, arr + 1)
  // Inferred RHS bounds: bounds(&value(temp((struct S){ 0 })), &value(temp((struct S){ 0 })) + 1)
  arr = &(struct S){ 0 };
  // CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '='
  // CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S>'
  // CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <BitCast>
  // CHECK:  `-UnaryOperator {{0x[0-9a-f]+}} 'struct S *' prefix '&' cannot overflow
  // CHECK:  `-CHKCBindTemporaryExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
  // CHECK:  `-CompoundLiteralExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue
  // CHECK:  `-InitListExpr {{0x[0-9a-f]+}} 'struct S':'struct S'
  // CHECK:  |-IntegerLiteral {{0x[0-9a-f]+}} 'int' 0
  // CHECK:  |-ImplicitValueInitExpr {{0x[0-9a-f]+}} '_Ptr<int>'
  // CHECK:  `-ImplicitValueInitExpr {{0x[0-9a-f]+}} '_Nt_array_ptr<char>'
  // CHECK: Target Bounds:
  // CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
  // CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
  // CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S>'
  // CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
  // CHECK:  |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' <LValueToRValue>
  // CHECK:  | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<struct S>' lvalue ParmVar {{0x[0-9a-f]+}} 'arr' '_Array_ptr<struct S>'
  // CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
  // CHECK: RHS Bounds:
  // CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
  // CHECK: |-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' prefix '&' cannot overflow
  // CHECK: | `-BoundsValueExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue _BoundTemporary
  // CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' '+'
  // CHECK:  |-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<struct S>' prefix '&' cannot overflow
  // CHECK:  | `-BoundsValueExpr {{0x[0-9a-f]+}} 'struct S':'struct S' lvalue _BoundTemporary
  // CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
}