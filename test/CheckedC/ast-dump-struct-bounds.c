// Tests for dumping of ASTS with Checked C extensions.
// This makes sure that additional information appears as
// expected.
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s

//===================================================================
// Dumps of different kinds of member bounds expressions
//===================================================================

struct S1 {
  _Array_ptr<int> f : count(5);
};

//CHECK: |-RecordDecl {{.*}} struct S1 definition
//CHECK: | `-FieldDecl {{.*}} f '_Array_ptr<int>'
//CHECK: |   `-CountBoundsExpr {{.*}} 'NULL TYPE' Element
//CHECK: |     `-IntegerLiteral {{.*}} 'int' 5

struct S2 {
  _Array_ptr<int> f : byte_count(5 * sizeof(int));
};

//CHECK: |-RecordDecl {{.*}} struct S2 definition
//CHECK: | `-FieldDecl {{.*}} f '_Array_ptr<int>'
//CHECK: |   `-CountBoundsExpr {{.*}} 'NULL TYPE' Byte
//CHECK: |     `-BinaryOperator {{.*}} 'unsigned {{.*}}' '*'
//CHECK: |       |-ImplicitCastExpr {{.*}} 'unsigned {{.*}}' <IntegralCast>
//CHECK: |       | `-IntegerLiteral {{.*}} 'int' 5
//CHECK: |       `-UnaryExprOrTypeTraitExpr {{.*}} 'unsigned {{.*}}' sizeof 'int'

struct S3 {
  _Array_ptr<int> f : bounds(f, f + 5);
};

//CHECK: |-RecordDecl {{.*}} struct S3 definition
//CHECK: | `-FieldDecl {{.*}} referenced f '_Array_ptr<int>'
//CHECK: |   `-RangeBoundsExpr {{.*}} 'NULL TYPE'
//CHECK: |     |-ImplicitCastExpr {{.*}} '_Array_ptr<int>' <LValueToRValue>
//CHECK: |     | `-DeclRefExpr {{.*}} '_Array_ptr<int>' lvalue Field {{0x[0-9a-f]+}} 'f' '_Array_ptr<int>'
//CHECK: |     `-BinaryOperator {{.*}} '_Array_ptr<int>' '+'
//CHECK: |       |-ImplicitCastExpr {{.*}} '_Array_ptr<int>' <LValueToRValue>
//CHECK: |       | `-DeclRefExpr {{.*}} '_Array_ptr<int>' lvalue Field {{0x[0-9a-f]+}} 'f' '_Array_ptr<int>'
//CHECK: |       `-IntegerLiteral {{.*}} 'int' 5

struct S4 {
  _Array_ptr<int> f : count(len);
  int len;
};

//CHECK: |-RecordDecl {{.*}} struct S4 definition
//CHECK: | |-FieldDecl {{.*}} f '_Array_ptr<int>'
//CHECK: | | `-CountBoundsExpr {{.*}} 'NULL TYPE' Element
//CHECK: | |   `-ImplicitCastExpr {{.*}} 'int' <LValueToRValue>
//CHECK: | |     `-DeclRefExpr {{.*}} 'int' lvalue Field {{0x[0-9a-f]+}} 'len' 'int'
//CHECK: | `-FieldDecl {{.*}} referenced len 'int'

struct S5 {
  _Array_ptr<int> f: byte_count(len * sizeof(int));
  int len;
};

//CHECK: |-RecordDecl {{.*}} struct S5 definition
//CHECK: | |-FieldDecl {{.*}} f '_Array_ptr<int>'
//CHECK: | | `-CountBoundsExpr {{.*}} 'NULL TYPE' Byte
//CHECK: | |   `-BinaryOperator {{.*}} 'unsigned {{.*}}' '*'
//CHECK: | |     |-ImplicitCastExpr {{.*}} 'unsigned {{.*}}' <IntegralCast>
//CHECK: | |     | `-ImplicitCastExpr {{.*}} 'int' <LValueToRValue>
//CHECK: | |     |   `-DeclRefExpr {{.*}} 'int' lvalue Field {{0x[0-9a-f]+}} 'len' 'int'
//CHECK: | |     `-UnaryExprOrTypeTraitExpr {{.*}} 'unsigned {{.*}}' sizeof 'int'
//CHECK: | `-FieldDecl {{.*}} referenced len 'int'

struct S6 {
  _Array_ptr<int> f : bounds(f, f + len);
  int len;
};

//CHECK: `-RecordDecl {{.*}} struct S6 definition
//CHECK: |-FieldDecl {{.*}} referenced f '_Array_ptr<int>'
//CHECK: | `-RangeBoundsExpr {{.*}} 'NULL TYPE'
//CHECK: |   |-ImplicitCastExpr {{.*}} '_Array_ptr<int>' <LValueToRValue>
//CHECK: |   | `-DeclRefExpr {{.*}} '_Array_ptr<int>' lvalue Field {{0x[0-9a-f]+}} 'f' '_Array_ptr<int>'
//CHECK: |   `-BinaryOperator {{.*}} '_Array_ptr<int>' '+'
//CHECK: |     |-ImplicitCastExpr {{.*}} '_Array_ptr<int>' <LValueToRValue>
//CHECK: |     | `-DeclRefExpr {{.*}} '_Array_ptr<int>' lvalue Field {{0x[0-9a-f]+}} 'f' '_Array_ptr<int>'
//CHECK: |     `-ImplicitCastExpr {{.*}} 'int' <LValueToRValue>
//CHECK: |       `-DeclRefExpr {{.*}} 'int' lvalue Field {{0x[0-9a-f]+}} 'len' 'int'
//CHECK: `-FieldDecl {{.*}} referenced len 'int'

