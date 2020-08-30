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
// RUN: %clang_cc1 -fcheckedc-extension -fdump-inferred-bounds -verify -verify-ignore-unexpected=warning -verify-ignore-unexpected=note -fdump-inferred-bounds %s | FileCheck %s

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

struct Interop_S1 {
  int *p : count(len);
  int len;
};

struct Interop_S2 {
  int arr[5] : itype(int _Checked[5]);
};

struct Interop_S4 {
  int *p : itype(_Ptr<int>);
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
// CHECK: `-MemberExpr {{0x[0-9a-f]+}} <col:35, col:38> 'int _Checked[5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK: `-DeclRefExpr {{0x[0-9a-f]+}} <col:35> 'struct S2':'struct S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct S2':'struct S2'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S2':'struct S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct S2':'struct S2'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S2':'struct S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct S2':'struct S2'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
}

//-------------------------------------------------------------------------//
// Test writing a member of a parameter                                    //
//-------------------------------------------------------------------------//

int global_arr1[5];
void f2(struct S1 a3) {
  // TODO: need bundled block.
  a3.p = global_arr1; // expected-error {{it is not possible to prove that the inferred bounds of a3.p imply the declared bounds of a3.p after assignment}}
  a3.len = 5;

// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-MemberExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue .p {{0x[0-9a-f]+}}
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct S1':'struct S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a3' 'struct S1':'struct S1'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <ArrayToPointerDecay>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr1' 'int [5]'
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
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr1' 'int [5]'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *':'int *' '+'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *':'int *' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr1' 'int [5]'
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
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
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1

}

//-------------------------------------------------------------------------//
// Test reading a member of a parameter with an interop type               //
//-------------------------------------------------------------------------//

// In an unchecked scope.
void f10(struct Interop_S1 a1, struct Interop_S2 b2,
         struct Interop_S4 c3) {
  _Array_ptr<int> ap : count(a1.len) = a1.p;

// CHECK: VarDecl {{.*}}  ap '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} <col:24, col:36> 'NULL TYPE' Element
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:30, col:33> 'int' <LValueToRValue>
// CHECK: |   `-MemberExpr {{0x[0-9a-f]+}} <col:30, col:33> 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} <col:30> 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:40, col:43> '_Array_ptr<int>' <BitCast>
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:40, col:43> 'int *' <LValueToRValue>
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} <col:40, col:43> 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} <col:40> 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: Initializer Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK:   | `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:   |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'

  _Array_ptr<int> bp : count(5) = b2.arr;

// CHECK: VarDecl {{.*}}  bp '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} <col:24, col:31> 'NULL TYPE' Element
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} <col:30> 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:35, col:38> '_Array_ptr<int>' <BitCast>
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:35, col:38> 'int *' <ArrayToPointerDecay>
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} <col:35, col:38> 'int [5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} <col:35> 'struct Interop_S2':'struct Interop_S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct Interop_S2':'struct Interop_S2'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <ArrayToPointerDecay>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int [5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S2':'struct Interop_S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct Interop_S2':'struct Interop_S2'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <ArrayToPointerDecay>
// CHECK:   | `-MemberExpr {{0x[0-9a-f]+}} 'int [5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK:   |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S2':'struct Interop_S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct Interop_S2':'struct Interop_S2'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5

  _Array_ptr<int> cp : count(1) = c3.p;

// CHECK: VarDecl {{.*}}  cp '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} <col:24, col:31> 'NULL TYPE' Element
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} <col:30> 'int' 1
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:35, col:38> '_Array_ptr<int>' <BitCast>
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:35, col:38> 'int *' <LValueToRValue>
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} <col:35, col:38> 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} <col:35> 'struct Interop_S4':'struct Interop_S4' lvalue ParmVar {{0x[0-9a-f]+}} 'c3' 'struct Interop_S4':'struct Interop_S4'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: Initializer Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: |   `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S4':'struct Interop_S4' lvalue ParmVar {{0x[0-9a-f]+}} 'c3' 'struct Interop_S4':'struct Interop_S4'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   |-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK:   | `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK:   |   `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:   |     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S4':'struct Interop_S4' lvalue ParmVar {{0x[0-9a-f]+}} 'c3' 'struct Interop_S4':'struct Interop_S4'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
}

// In a checked scope.
_Checked void f11(struct Interop_S1 a1, struct Interop_S2 b2,
                  struct Interop_S4 c3) {
  _Array_ptr<int> ap : count(a1.len) = a1.p;

// CHECK: VarDecl {{.*}}  '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} <col:24, col:36> 'NULL TYPE' Element
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:30, col:33> 'int' <LValueToRValue>
// CHECK: |   `-MemberExpr {{0x[0-9a-f]+}} <col:30, col:33> 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} <col:30> 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:40, col:43> '_Array_ptr<int>' <LValueToRValue>
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:40, col:43> '_Array_ptr<int>' lvalue <LValueBitCast> BoundsSafeInterface
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} <col:40, col:43> 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} <col:40> 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:   `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK:   | `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:   |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'

  _Array_ptr<int> bp : count(5) = b2.arr;

// CHECK: VarDecl {{.*}}  '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} <col:24, col:31> 'NULL TYPE' Element
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} <col:30> 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:35, col:38> '_Array_ptr<int>' <ArrayToPointerDecay> BoundsSafeInterface
// CHECK:   `-MemberExpr {{0x[0-9a-f]+}} <col:35, col:38> 'int [5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} <col:35> 'struct Interop_S2':'struct Interop_S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct Interop_S2':'struct Interop_S2'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: Initializer Bounds:
 // CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay> BoundsSafeInterface
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int [5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S2':'struct Interop_S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct Interop_S2':'struct Interop_S2'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay> BoundsSafeInterface
// CHECK:   | `-MemberExpr {{0x[0-9a-f]+}} 'int [5]' lvalue .arr {{0x[0-9a-f]+}}
// CHECK:   |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S2':'struct Interop_S2' lvalue ParmVar {{0x[0-9a-f]+}} 'b2' 'struct Interop_S2':'struct Interop_S2'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5

  _Array_ptr<int> cp : count(1) = c3.p;

// CHECK: {{.*}}  cp '_Array_ptr<int>' cinit
// CHECK: |-CountBoundsExpr {{0x[0-9a-f]+}} <col:24, col:31> 'NULL TYPE' Element
// CHECK: | `-IntegerLiteral {{0x[0-9a-f]+}} <col:30> 'int' 1
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:35, col:38> '_Array_ptr<int>' <BitCast>
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:35, col:38> '_Ptr<int>' <LValueToRValue>
// CHECK:     `-ImplicitCastExpr {{0x[0-9a-f]+}} <col:35, col:38> '_Ptr<int>' lvalue <LValueBitCast> BoundsSafeInterface
// CHECK:       `-MemberExpr {{0x[0-9a-f]+}} <col:35, col:38> 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:         `-DeclRefExpr {{0x[0-9a-f]+}} <col:35> 'struct Interop_S4':'struct Interop_S4' lvalue ParmVar {{0x[0-9a-f]+}} 'c3' 'struct Interop_S4':'struct Interop_S4'
// CHECK: Declared Bounds:
// CHECK: CountBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE' Element
// CHECK: `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: Initializer Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<int>' <LValueToRValue>
// CHECK: |   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<int>' lvalue <LValueBitCast> BoundsSafeInterface
// CHECK: |     `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |       `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S4':'struct Interop_S4' lvalue ParmVar {{0x[0-9a-f]+}} 'c3' 'struct Interop_S4':'struct Interop_S4'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   |-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK:   | `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<int>' <LValueToRValue>
// CHECK:   |   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<int>' lvalue <LValueBitCast> BoundsSafeInterface
// CHECK:   |     `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:   |       `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S4':'struct Interop_S4' lvalue ParmVar {{0x[0-9a-f]+}} 'c3' 'struct Interop_S4':'struct Interop_S4'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
}

int global_arr2 _Checked[5];

void f12(struct Interop_S1 a1) {
  // TODO: need bundled block.
  a1.p = global_arr2; // expected-error {{it is not possible to prove that the inferred bounds of a1.p imply the declared bounds of a1.p after assignment}}
  a1.len = 5;
}

// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int *' '='
// CHECK: |-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <BitCast> BoundsSafeInterface
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK:   | `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:   |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: RHS Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5

_Checked void f13(struct Interop_S1 a1) {
  // TODO: need bundled block.
  a1.p = global_arr2; // expected-error {{it is not possible to prove that the inferred bounds of a1.p imply the declared bounds of a1.p after assignment}}
  a1.len = 5;
}

// CHECK: BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '='
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' lvalue <LValueBitCast> BoundsSafeInterface
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:   `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: | `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} 'int *' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK:   | `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:   |   `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int' <LValueToRValue>
// CHECK:     `-MemberExpr {{0x[0-9a-f]+}} 'int' lvalue .len {{0x[0-9a-f]+}}
// CHECK:       `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S1':'struct Interop_S1' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S1':'struct Interop_S1'
// CHECK: RHS Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5

void f14(struct Interop_S4 a1) {
  a1.p = global_arr2;
}

// CHECK: BinaryOperator {{0x[0-9a-f]+}} 'int *' '='
// CHECK: |-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S4':'struct Interop_S4' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S4':'struct Interop_S4'
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <BitCast> BoundsSafeInterface
// CHECK:   `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:     `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK: Target Bounds:
// CHECK: RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK: | `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK: |   `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK: |     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S4':'struct Interop_S4' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S4':'struct Interop_S4'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>' '+'
// CHECK:   |-CStyleCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <BitCast>
// CHECK:   | `-ImplicitCastExpr {{0x[0-9a-f]+}} 'int *' <LValueToRValue>
// CHECK:   |   `-MemberExpr {{0x[0-9a-f]+}} 'int *' lvalue .p {{0x[0-9a-f]+}}
// CHECK:   |     `-DeclRefExpr {{0x[0-9a-f]+}} 'struct Interop_S4':'struct Interop_S4' lvalue ParmVar {{0x[0-9a-f]+}} 'a1' 'struct Interop_S4':'struct Interop_S4'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 1
// CHECK: RHS Bounds:
// CHECK:  RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK: `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK:   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK:   `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5

_Checked void f15(struct Interop_S4 a1) {
  a1.p = global_arr2;
}

// CHECK: ImplicitCastExpr {{0x[0-9a-f]+}} '_Ptr<int>' <BitCast>
// CHECK: |-Inferred SubExpr Bounds
// CHECK: | `-RangeBoundsExpr {{0x[0-9a-f]+}} 'NULL TYPE'
// CHECK: |   |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |   | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK: |   `-BinaryOperator {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
// CHECK: |     |-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>':'_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK: |     | `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
// CHECK: |     `-IntegerLiteral {{0x[0-9a-f]+}} 'int' 5
// CHECK: `-ImplicitCastExpr {{0x[0-9a-f]+}} '_Array_ptr<int>' <ArrayToPointerDecay>
// CHECK:   `-DeclRefExpr {{0x[0-9a-f]+}} 'int _Checked[5]' lvalue Var {{0x[0-9a-f]+}} 'global_arr2' 'int _Checked[5]'
