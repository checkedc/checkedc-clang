// Tests of inferred bounds for expressions that do pointer arithmetic.
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
// Test post-increment of a pointer that is then dereferenced.             //
//-------------------------------------------------------------------------//

double f1(_Array_ptr<double> a : bounds(a, a + len), int len) {
  double sum = 0;
  _Array_ptr<double> p : bounds(a, a + len) = a;
  _Array_ptr<double> end = a + len;
  while (p < end) {
    sum += *p++;

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'double' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'double' lvalue prefix '*'
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |     `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   |       `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'len' 'int'
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' postfix '++'
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<double>'

  }
  return sum;
}

//-------------------------------------------------------------------------//
// Test post-decrement of a pointer that is then dereferenced.             //
//-------------------------------------------------------------------------//

double f2(_Array_ptr<double> a : count(2)) {
  _Array_ptr<double> p : bounds(a, a + 2) = a + 1;
  return *(p--);

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'double' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'double' lvalue prefix '*'
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK:   `-ParenExpr {{0x[0-9a-f]+}} '_Array_ptr<double>'
// CHECK:     `-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' postfix '--'
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<double>'
}

//-------------------------------------------------------------------------//
// Test pre-increment of a pointer that is then dereferenced.             //
//-------------------------------------------------------------------------//

double f3(_Array_ptr<double> a : count(2)) {
 _Array_ptr<double> p : bounds(a, a + 2) = a;
 return *(++p);

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'double' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'double' lvalue prefix '*'
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK:   `-ParenExpr {{0x[0-9a-f]+}} '_Array_ptr<double>'
// CHECK:     `-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' prefix '++'
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<double>'

}

//-------------------------------------------------------------------------//
// Test pre-decrement of a pointer that is then dereferenced.              //
//-------------------------------------------------------------------------//

double f4(_Array_ptr<double> a : count(2)) {
  _Array_ptr<double> p : bounds(a, a + 2) = a + 1;
  return *(--p);

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'double' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'double' lvalue prefix '*'
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK:   `-ParenExpr {{0x[0-9a-f]+}} '_Array_ptr<double>'
// CHECK:     `-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' prefix '--'
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<double>'
}

//-------------------------------------------------------------------------//
// Test pointer addition that is dereferenced.                             //
//-------------------------------------------------------------------------//

double f5(_Array_ptr<double> a : bounds(a, a + len), int len) {
  double sum = 0;
  _Array_ptr<double> p : bounds(a, a + len) = a;
  for (int i = 0; i < len; i++) 
    sum += *(p + i);

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'double' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'double' lvalue prefix '*'
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |     `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   |       `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'len' 'int'
// CHECK:   `-ParenExpr {{0x[0-9a-f]+}} '_Array_ptr<double>'
// CHECK:     `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+'
// CHECK:       |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:       | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<double>'
// CHECK:       `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:         `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue Var {{0x[0-9a-f]+}} 'i' 'int'

  return sum;
}

//-------------------------------------------------------------------------//
// Test pointer subtraction that is dereferenced.                          //
//-------------------------------------------------------------------------//

double f6(_Array_ptr<double> a : bounds(a, a + len), int len) {
  double sum = 0;
  _Array_ptr<double> p : bounds(a, a + len) = p + len - 1;
  for (int i = 0; i < len; i++)
    sum += *(p - i);

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'double' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'double' lvalue prefix '*'
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |     `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   |       `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue ParmVar {{0x[0-9a-f]+}} 'len' 'int'
// CHECK:   `-ParenExpr {{0x[0-9a-f]+}} '_Array_ptr<double>'
// CHECK:     `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '-'
// CHECK:       |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:       | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<double>'
// CHECK:       `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:         `-DeclRefExpr {{0x[0-9a-f]+}} 'int' lvalue Var {{0x[0-9a-f]+}} 'i' 'int'

  return sum;
}

//-------------------------------------------------------------------------//
// Test compound assignment addition that is then dereferenced.            //
//-------------------------------------------------------------------------//

double f7(_Array_ptr<double> a : count(2)) {
  _Array_ptr<double> p : bounds(a, a + 2) = a;
  return *(p += 1);
}

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'double' <LValueToRValue>
// CHECK:   `-UnaryOperator {{0x[0-9a-f]+}} 'double' lvalue prefix '*'
// CHECK:     |-Bounds
// CHECK:     | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:     |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:     |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:     |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+'
// CHECK:     |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:     |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:     |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK:    `-ParenExpr {{0x[0-9a-f]+}} '_Array_ptr<double>'
// CHECK:        `-CompoundAssignOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+=' ComputeLHSTy='_Array_ptr<double>' ComputeResultTy='_Array_ptr<double>'
// CHECK:          |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<double>'
// CHECK:          `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

//-------------------------------------------------------------------------//
// Test compound assignment subtraction that is then dereferenced.         //
//-------------------------------------------------------------------------//

double f8(_Array_ptr<double> a : count(2)) {
  _Array_ptr<double> p : bounds(a, a + 2) = a + 1;
  return *(p -= 1);

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} 'double' <LValueToRValue>
// CHECK: `-UnaryOperator {{0x[0-9a-f]+}} 'double' lvalue prefix '*'
// CHECK:   |-Bounds
// CHECK:   | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK:   |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |   | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '+'
// CHECK:   |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' <LValueToRValue>
// CHECK:   |     | `-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Array_ptr<double>'
// CHECK:   |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 2
// CHECK:   `-ParenExpr {{0x[0-9a-f]+}} '_Array_ptr<double>'
// CHECK:     `-CompoundAssignOperator {{0x[0-9a-f]+}} '_Array_ptr<double>' '-=' ComputeLHSTy='_Array_ptr<double>' ComputeResultTy='_Array_ptr<double>'
// CHECK:       |-DeclRefExpr {{0x[0-9a-f]+}} '_Array_ptr<double>' lvalue Var {{0x[0-9a-f]+}} 'p' '_Array_ptr<double>'
// CHECK:       `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
}
