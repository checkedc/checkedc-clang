// Tests of inferred bounds for references of members of structures or
// unions.
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

struct S1 {
  _Array_ptr<int> p : count(len);
  int len;
};

struct S2 {
  int arr _Checked[5];
};

struct S3 {
  int f;
};

//-------------------------------------------------------------------------//
// Test reading a member of a parameter                                    //
//-------------------------------------------------------------------------//

void f1(struct S1 a1, struct S2 b2) {
  _Array_ptr<int> ap : count(a1.len) = a1.p;

// CHECK: VarDecl {{.*}}  ap '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} <col:24, col:36> 'NULL TYPE' Element
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:30, col:33> 'int' <LValueToRValue>
// CHECK: |   `-MemberExpr {{0x[0-9a-f]+}} <col:30, col:33> 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} <col:30> 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct S1':'struct S1'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:40, col:43> '_Array_ptr<int>' <LValueToRValue>
// CHECK: `-MemberExpr {{0x[0-9a-f]+}} <col:40, col:43> '_Array_ptr<int>' lvalue .p {{0x[0-9a-f]+}}
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} <col:40> 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct S1':'struct S1'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct S1':'struct S1'
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct S1':'struct S1'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct S1':'struct S1'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct S1':'struct S1'

  _Array_ptr<int> bp : count(5) = b2.arr;

// CHECK: VarDecl {{.*}}  bp '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} <col:24, col:31> 'NULL TYPE' Element
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} <col:30> 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:35, col:38> '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: `-MemberExpr {{0x[0-9a-f]+}} <col:35, col:38> 'int checked[5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} <col:35> 'struct S2':'struct S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct S2':'struct S2'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int checked[5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S2':'struct S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct S2':'struct S2'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int checked[5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S2':'struct S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct S2':'struct S2'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'unsigned long long' 5
}

//-------------------------------------------------------------------------//
// Test writing a member of a parameter                                    //
//-------------------------------------------------------------------------//

void f2(struct S1 a3) {
  int local_arr1[5];
  // TODO: need bundled block.
  a3.p = local_arr1;

// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-MemberExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue .p {{0x[0-9a-f]+}}
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a3' 'struct S1':'struct S1'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <ArrayToPointerDecay>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'local_arr1' 'int [5]'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a3' 'struct S1':'struct S1'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <LValueToRValue>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue .p {{0x[0-9a-f]+}}
// CHECK:   |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a3' 'struct S1':'struct S1'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK: `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a3' 'struct S1':'struct S1'
// CHECK: RHS Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'local_arr1' 'int [5]'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *':'int *' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'local_arr1' 'int [5]'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'unsigned long long' 5

  a3.len = 5;
}

//-------------------------------------------------------------------------//
// Test taking the address of a member of a parameter                      //
//-------------------------------------------------------------------------//

void f3(struct S3 c1) {
  _Array_ptr<int> cp : bounds(&c1.f, &c1.f + 1) = &c1.f;

// CHECK: VarDecl {{.*}}  cp '_Array_ptr<int>' cinit
// CHECK: |-RangeBoundsExpr {{.*}}  'NULL TYPE'
// CHECK: | |-UnaryOperator {{.*}}  'int *' prefix '&'
// CHECK: | | `-MemberExpr {{.*}}  'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: | |   `-DeclRefExpr {{.*}}  'struct S3':'struct S3' lvalue ParmVar {{0x[0-9a-f]+}} 'c1' 'struct S3':'struct S3'
// CHECK: | `-BinaryOperator {{.*}}  'int *' '+'
// CHECK: |   |-UnaryOperator {{.*}}  'int *' prefix '&'
// CHECK: |   | `-MemberExpr {{.*}}  'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   |   `-DeclRefExpr {{.*}}  'struct S3':'struct S3' lvalue ParmVar {{0x[0-9a-f]+}} 'c1' 'struct S3':'struct S3'
// CHECK: |   `-IntegerLiteral {{.*}}  'int' 1
// CHECK: `-ImplicitCastExpr {{.*}}  '_Array_ptr<int>' <BitCast>
// CHECK: `-UnaryOperator {{.*}}  'int *' prefix '&'
// CHECK: `-MemberExpr {{.*}}  'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: `-DeclRefExpr {{.*}}  <col:52> 'struct S3':'struct S3' lvalue ParmVar {{0x[0-9a-f]+}} 'c1' 'struct S3':'struct S3'
// CHECK: Declared Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S3':'struct S3' lvalue ParmVar {{0x[0-9a-f]+}} 'c1' 'struct S3':'struct S3'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} 'int *' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S3':'struct S3' lvalue ParmVar {{0x[0-9a-f]+}} 'c1' 'struct S3':'struct S3'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S3':'struct S3' lvalue ParmVar {{0x[0-9a-f]+}} 'c1' 'struct S3':'struct S3'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-UnaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' prefix '&'
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .f {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S3':'struct S3' lvalue ParmVar {{0x[0-9a-f]+}} 'c1' 'struct S3':'struct S3'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'unsigned long long' 1

}
