// Tests of inferred bounds for expressions in assignments and declarations.
// The goal is to check that the bounds are being inferred correctly.  
//
// The tests have the general form:
// 1. Some C code.
// 2. A description of the inferred bounds for that C code:
//  a. The source assignnment or declaration.
//  b. The expected bounds.
//  c. The inferred bounds.
// The description uses AST dumps.
//
// This line is for the clang test infrastructure:
// RUN: %clang_cc1 -fcheckedc-extension -verify -fdump-inferred-bounds %s | FileCheck %s

//-------------------------------------------------------------------------//
// Test assignment of integers to _Array_ptr variables.  This covers both  //
// 0 (NULL) and non-zero integers (the results of casts).                  //
//-------------------------------------------------------------------------//

// First test the null case.  Also test different ways of declaring the same
// bounds for the lhs of an assignment
void f1(_Array_ptr<int> a : bounds(a, a + 5)) {
  a = 0;
}

// CHECK: Assignment:
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <NullToPointer>
// CHECK:  `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 0
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: RHS Bounds:
// CHECK:  NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'Any

void f2(_Array_ptr<int> b : count(5)) {
  b = 0;
}

// CHECK: Assignment:
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <NullToPointer>
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 0
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: RHS Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'Any

void f3(_Array_ptr<int> c : byte_count(sizeof(int) * 5)) {
  c = 0;
}

// CHECK: Assignment:
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'c' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <NullToPointer>
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 0
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<char>' <BitCast>
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'c' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<char>' '+'
// CHECK: |-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<char>' <BitCast>
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'c' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'unsigned int' '*'
// CHECK: |-UnaryExprOrTypeTraitExpr {{0x[0-9a-f]+}} 'unsigned int' sizeof 'int'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'unsigned int' <IntegralCast>
// CHECK:       `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: RHS Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'Any

//  Test assignment of an integer constant expressed as
// an enum constant.
enum E1 {
  EnumVal1,
  EnumVal2
};

void f4(_Array_ptr<int> d : count(5)) {
  d = (_Array_ptr<int>) EnumVal1;
}

// CHECK: Assignment:
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'd' '_Array_ptr<int>'
// CHECK: `-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <NullToPointer>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' EnumConstant {{0x[0-9a-f]+}} 'EnumVal1' 'int'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'd' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'd' '_Array_ptr<int>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: RHS Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'Any

void f5(void) {
  _Array_ptr<int> d : count(5) = 0;
}

// Now test the non-null case.  Also test different ways of declaring the same
// bounds for the lhs of an assignment

// CHECK: Declaration:
// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} d '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Element
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <NullToPointer>
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 0
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: Initializer Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'Any

void f6(_Array_ptr<int> a : bounds(a, a + 5)) {
  a = (_Array_ptr<int>) 5; // expected-error {{expression has no bounds}}
}

// CHECK: Assignment:
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <IntegralToPointer>
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: RHS Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' None

void f7(void) {
  _Array_ptr<int> d : count(5) = (_Array_ptr<int>) 5; // expected-error {{expression has no bounds}}
}

// CHECK: Declaration:
// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} d '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Element
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 5
// CHECK:`-CStyleCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <IntegralToPointer>
// CHECK:`-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 5
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: Initializer Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' None

//-------------------------------------------------------------------------//
// Test assignment of variables to _Array_ptr variables.  This covers both //
// variables with bounds and variables without bounds.                     //
//-------------------------------------------------------------------------//

void f20(_Array_ptr<int> a : count(len),
         _Array_ptr<int> b : count(len),
         int len) {
  a = b;
}

// CHECK: Assignment:
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'len' 'int'
// CHECK: RHS Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'len' 'int'

void f21(_Array_ptr<int> a : count(5),
         _Array_ptr<int> b) {
  a = b;  // expected-error {{expression has no bounds}}
}

// CHECK: Assignment:
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: RHS Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' None

// Only test declarations for the negative case (where an error is expected}
void f22(_Array_ptr<int> b) {
  _Array_ptr<int> a : count(5) = b;  // expected-error {{expression has no bounds}}
}

// CHECK: Declaration:
// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} a '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Element
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: Initializer Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' None

//-------------------------------------------------------------------------//
// Test assignment of arrays to _Array_ptr variables with bounds           //
//-------------------------------------------------------------------------//

void f30(_Array_ptr<int> a : count(3)) {
  int arr[5];
  a = arr;
}

// CHECK: Assignment:
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <ArrayToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int [5]'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: RHS Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int [5]'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *':'int *' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int [5]'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'unsigned long long' 5

void f31(void) {
  int arr[5];
  _Array_ptr<int> b : count(3) = arr;
}

// CHECK: Declaration:
// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Element
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 3
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <BitCast>
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *' <ArrayToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int [5]'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 3
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int [5]'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *':'int *' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'arr' 'int [5]'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'unsigned long long' 5

