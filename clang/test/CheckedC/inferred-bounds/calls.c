// Tests of inferred bounds for expressions in assignments and declarations.
// The goal is to check that the bounds are being inferred correctly.  This
// file covers:
// - Function calls returning an _Array_ptr where arguments have to be
//   substituted in the bounds.
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

// In all these functions, though they take two arguments, the bounds
// are only expressed in terms of the first of the two.
// This means we can test that substitution is successful even if
// a non-modifying-expression is used for the second argument.

extern _Array_ptr<int> f_bounds(_Array_ptr<int> start, _Array_ptr<int> end) : bounds(start, start + 1);
extern _Array_ptr<int> f_count(int i, int j) : count(i);
extern _Array_ptr<int> f_byte(int i, int j) : byte_count(i);

extern int* f_boundsi(int* start, int* end) : bounds(start, start + 1);
extern int* f_counti(int i, int j) : count(i);
extern int* f_bytei(int i, int j) : byte_count(i);

//
// Performing Substitution
//

void f0(_Array_ptr<int> a) {
    _Array_ptr<int> b : bounds(a, a+1) = f_bounds(a, a);
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: CallExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (*)(_Array_ptr<int>, _Array_ptr<int>) : bounds(arg #0, arg #0 + 1)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (_Array_ptr<int>, _Array_ptr<int>) : bounds(arg #0, arg #0 + 1)' Function {{0x[0-9a-f]+}} 'f_bounds' '_Array_ptr<int> (_Array_ptr<int>, _Array_ptr<int>) : bounds(arg #0, arg #0 + 1)'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: Declared Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

void f1(int i) {
    _Array_ptr<int> b : count(i) = f_count(i, i);
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Element
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: CHKCBindTemporaryExpr [[TEMP1:0x[0-9a-f]+]] {{.*}} '_Array_ptr<int>'
// CHECK:  CallExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>'
// CHECK:    ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (*)(int, int) : count(arg #0)' <FunctionToPointerDecay>
// CHECK:    `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (int, int) : count(arg #0)' Function {{0x[0-9a-f]+}} 'f_count' '_Array_ptr<int> (int, int) : count(arg #0)'
// CHECK:    ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK:    `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK:    ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK:   `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   BoundsValueExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' _BoundTemporary  [[TEMP1]]
// CHECK:   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   BoundsValueExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' _BoundTemporary  [[TEMP1]]
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'

void f2(int i) {
    _Array_ptr<int> b : byte_count(i) = f_byte(i, i);
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: CHKCBindTemporaryExpr [[TEMP2:0x[0-9a-f]+]] {{.*}} '_Array_ptr<int>'
// CHECK: CallExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (*)(int, int) : byte_count(arg #0)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (int, int) : byte_count(arg #0)' Function {{0x[0-9a-f]+}} 'f_byte' '_Array_ptr<int> (int, int) : byte_count(arg #0)'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<char>' <BitCast>
// CHECK:     BoundsValueExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' _BoundTemporary  [[TEMP2]]
// CHECK:   BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<char>' '+'
// CHECK:     CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<char>' <BitCast>
// CHECK:       BoundsValueExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' _BoundTemporary  [[TEMP2]]
// CHECK:         ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:           DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'


//
// Modifying Expressions
//

void f10(_Array_ptr<int> a, _Array_ptr<int> b) {
    _Array_ptr<int> c : bounds(a, a+1) = f_bounds(a, b++);
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} c '_Array_ptr<int>' cinit
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: CallExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (*)(_Array_ptr<int>, _Array_ptr<int>) : bounds(arg #0, arg #0 + 1)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (_Array_ptr<int>, _Array_ptr<int>) : bounds(arg #0, arg #0 + 1)' Function {{0x[0-9a-f]+}} 'f_bounds' '_Array_ptr<int> (_Array_ptr<int>, _Array_ptr<int>) : bounds(arg #0, arg #0 + 1)'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: Declared Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

void f11(_Array_ptr<int> a, _Array_ptr<int> b) {
    _Array_ptr<int> c : bounds(a, a+1) = f_bounds(b++, a); // \
    // expected-error {{expression not allowed in argument for parameter used in function return bounds}} \
    // expected-error {{inferred bounds for 'c' are unknown after initialization}} \
    // expected-note {{(expanded) declared bounds are 'bounds(a, a + 1)'}}
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} c '_Array_ptr<int>' cinit
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: CallExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (*)(_Array_ptr<int>, _Array_ptr<int>) : bounds(arg #0, arg #0 + 1)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (_Array_ptr<int>, _Array_ptr<int>) : bounds(arg #0, arg #0 + 1)' Function {{0x[0-9a-f]+}} 'f_bounds' '_Array_ptr<int> (_Array_ptr<int>, _Array_ptr<int>) : bounds(arg #0, arg #0 + 1)'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'b' '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: Declared Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<int>'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: Initializer Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Invalid

void f12(int i, int j) {
    _Array_ptr<int> b : count(i) = f_count(i, j++);
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Element
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: CHKCBindTemporaryExpr [[TEMP3:0x[0-9a-f]+]] {{.*}} '_Array_ptr<int>'
// CHECK: CallExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (*)(int, int) : count(arg #0)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (int, int) : count(arg #0)' Function {{0x[0-9a-f]+}} 'f_count' '_Array_ptr<int> (int, int) : count(arg #0)'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:    BoundsValueExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' _BoundTemporary  [[TEMP3]]
// CHECK:    BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:      BoundsValueExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' _BoundTemporary  [[TEMP3]]
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:       DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'

void f13(int i, int j) {
    _Array_ptr<int> b : count(i) = f_count(j++, i); // \
    // expected-error {{expression not allowed in argument for parameter used in function return bounds}} \
    // expected-error {{inferred bounds for 'b' are unknown after initialization}} \
    // expected-note {{(expanded) declared bounds are 'bounds(b, b + i)'}}
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Element
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: CallExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (*)(int, int) : count(arg #0)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (int, int) : count(arg #0)' Function {{0x[0-9a-f]+}} 'f_count' '_Array_ptr<int> (int, int) : count(arg #0)'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Invalid

void f14(int i, int j) {
    _Array_ptr<int> b : byte_count(i) = f_byte(i, j++);
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: CHKCBindTemporaryExpr [[TEMP4:0x[0-9a-f]+]] {{.*}} '_Array_ptr<int>'
// CHECK: CallExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (*)(int, int) : byte_count(arg #0)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (int, int) : byte_count(arg #0)' Function {{0x[0-9a-f]+}} 'f_byte' '_Array_ptr<int> (int, int) : byte_count(arg #0)'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<char>' <BitCast>
// CHECK:     BoundsValueExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' _BoundTemporary  [[TEMP4]]
// CHECK:   BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<char>' '+'
// CHECK:    CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<char>' <BitCast>
// CHECK:      BoundsValueExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' _BoundTemporary  [[TEMP4]]
// CHECK:    ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:      DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'

void f15(int i, int j) {
    _Array_ptr<int> b : byte_count(i) = f_byte(j++, i); // \
    // expected-error {{expression not allowed in argument for parameter used in function return bounds}} \
    // expected-error {{inferred bounds for 'b' are unknown after initialization}} \
    // expected-note {{(expanded) declared bounds are 'bounds((_Array_ptr<char>)b, (_Array_ptr<char>)b + i)'}}
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: CallExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (*)(int, int) : byte_count(arg #0)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int> (int, int) : byte_count(arg #0)' Function {{0x[0-9a-f]+}} 'f_byte' '_Array_ptr<int> (int, int) : byte_count(arg #0)'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Invalid

// Interoperation Types

void f20(int* a, int* b) {
    _Array_ptr<int> c : bounds(a, a+1) = f_boundsi(a, b++);
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} c '_Array_ptr<int>' cinit
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <BitCast>
// CHECK: `-CallExpr {{0x[0-9a-f]+}} {{.*}} 'int *'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *(*)(int *, int *) : bounds(arg #0, arg #0 + 1) itype(_Array_ptr<int>)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *(int *, int *) : bounds(arg #0, arg #0 + 1) itype(_Array_ptr<int>)' Function {{0x[0-9a-f]+}} 'f_boundsi' 'int *(int *, int *) : bounds(arg #0, arg #0 + 1) itype(_Array_ptr<int>)'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'b' 'int *'
// CHECK: Declared Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

void f21(int* a, int* b) {
    _Array_ptr<int> c : bounds(a, a+1) = f_boundsi(b++, a); // \
    // expected-error {{expression not allowed in argument for parameter used in function return bounds}} \
    // expected-error {{inferred bounds for 'c' are unknown after initialization}} \
    // expected-note {{(expanded) declared bounds are 'bounds(a, a + 1)'}}
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} c '_Array_ptr<int>' cinit
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 1
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <BitCast>
// CHECK: `-CallExpr {{0x[0-9a-f]+}} {{.*}} 'int *'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *(*)(int *, int *) : bounds(arg #0, arg #0 + 1) itype(_Array_ptr<int>)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *(int *, int *) : bounds(arg #0, arg #0 + 1) itype(_Array_ptr<int>)' Function {{0x[0-9a-f]+}} 'f_boundsi' 'int *(int *, int *) : bounds(arg #0, arg #0 + 1) itype(_Array_ptr<int>)'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int *' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'b' 'int *'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: Declared Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'int *'
// CHECK: IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: Initializer Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Invalid

void f22(int i, int j) {
    _Array_ptr<int> b : count(i) = f_counti(i, j++);
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Element
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <BitCast>
// CHECK: CHKCBindTemporaryExpr [[TEMP5:0x[0-9a-f]+]] {{.*}} 'int *'
// CHECK: `-CallExpr {{0x[0-9a-f]+}} {{.*}} 'int *'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *(*)(int, int) : count(arg #0) itype(_Array_ptr<int>)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *(int, int) : count(arg #0) itype(_Array_ptr<int>)' Function {{0x[0-9a-f]+}} 'f_counti' 'int *(int, int) : count(arg #0) itype(_Array_ptr<int>)'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:    BoundsValueExpr {{0x[0-9a-f]+}} 'int *' _BoundTemporary  [[TEMP5]]
// CHECK:    BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK:      BoundsValueExpr {{0x[0-9a-f]+}} 'int *' _BoundTemporary  [[TEMP5]]
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:       DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'

void f23(int i, int j) {
    _Array_ptr<int> b : count(i) = f_counti(j++, i); // \
    // expected-error {{expression not allowed in argument for parameter used in function return bounds}} \
    // expected-error {{inferred bounds for 'b' are unknown after initialization}} \
    // expected-note {{(expanded) declared bounds are 'bounds(b, b + i)'}}
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Element
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <BitCast>
// CHECK: `-CallExpr {{0x[0-9a-f]+}} {{.*}} 'int *'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *(*)(int, int) : count(arg #0) itype(_Array_ptr<int>)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *(int, int) : count(arg #0) itype(_Array_ptr<int>)' Function {{0x[0-9a-f]+}} 'f_counti' 'int *(int, int) : count(arg #0) itype(_Array_ptr<int>)'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Invalid

void f24(int i, int j) {
    _Array_ptr<int> b : byte_count(i) = f_bytei(i, j++);
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <BitCast>
// CHECK: CHKCBindTemporaryExpr [[TEMP6:0x[0-9a-f]+]] {{.*}} 'int *'
// CHECK: `-CallExpr {{0x[0-9a-f]+}} {{.*}} 'int *'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *(*)(int, int) : byte_count(arg #0)  itype(_Array_ptr<int>)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *(int, int) : byte_count(arg #0) itype(_Array_ptr<int>)' Function {{0x[0-9a-f]+}} 'f_bytei' 'int *(int, int) : byte_count(arg #0) itype(_Array_ptr<int>)'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:    CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<char>' <BitCast>
// CHECK:      BoundsValueExpr {{0x[0-9a-f]+}} 'int *' _BoundTemporary [[TEMP6]]
// CHECK:    BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<char>' '+'
// CHECK:      CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<char>' <BitCast>
// CHECK:        BoundsValueExpr {{0x[0-9a-f]+}} 'int *' _BoundTemporary [[TEMP6]]
// CHECK:      ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:        DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'

void f25(int i, int j) {
    _Array_ptr<int> b : byte_count(i) = f_bytei(j++, i); // \
    // expected-error {{expression not allowed in argument for parameter used in function return bounds}} \
    // expected-error {{inferred bounds for 'b' are unknown after initialization}} \
    // expected-note {{(expanded) declared bounds are 'bounds((_Array_ptr<char>)b, (_Array_ptr<char>)b + i)'}}
}

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} b '_Array_ptr<int>' cinit
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} {{.*}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Array_ptr<int>' <BitCast>
// CHECK: `-CallExpr {{0x[0-9a-f]+}} {{.*}} 'int *'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int *(*)(int, int) : byte_count(arg #0) itype(_Array_ptr<int>)' <FunctionToPointerDecay>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int *(int, int) : byte_count(arg #0)  itype(_Array_ptr<int>)' Function {{0x[0-9a-f]+}} 'f_bytei' 'int *(int, int) : byte_count(arg #0) itype(_Array_ptr<int>)'
// CHECK: UnaryOperator {{0x[0-9a-f]+}} {{.*}} 'int' postfix '++'
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'j' 'int'
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Byte
// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'i' 'int'
// CHECK: Initializer Bounds:
// CHECK: NullaryBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Invalid
