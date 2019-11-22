// Tests for dumping of ASTs of Checked C's bounds-safe information
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s


// No bounds-safe information
void f1(int *a, int n) {
  // CHECK: FunctionDecl {{.*}} f1 'void (int *, int)'
  // CHECK-NEXT: ParmVarDecl {{.*}} a 'int *'
  // CHECK-NEXT: ParmVarDecl {{.*}} n 'int'
  // CHECK-NEXT: CompoundStmt
}

// bounds-safe information on the definition
void f2(int *a : count(n), int n) {
  // CHECK: FunctionDecl {{.*}} f2 'void (int * : count(arg #1) itype(_Array_ptr<int>), int)'
  // CHECK-NEXT: ParmVarDecl {{.*}} a 'int *'
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT: ImplicitCastExpr
  // CHECK-NEXT: DeclRefExpr {{.*}} 'int' lvalue ParmVar {{.*}} 'n' 'int'
  // CHECK-NEXT: InteropTypeExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: PointerType {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: BuiltinType {{.*}} 'int'
  // CHECK-NEXT: ParmVarDecl {{.*}} n 'int'
  // CHECK-NEXT: CompoundStmt
}


// bounds-safe information on redeclaration
void f3(int *a : count(n), int n);
  // CHECK: FunctionDecl {{.*}} f3 'void (int * : count(arg #1) itype(_Array_ptr<int>), int)'
  // CHECK-NEXT: ParmVarDecl {{.*}} a 'int *'
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT: ImplicitCastExpr
  // CHECK-NEXT: DeclRefExpr {{.*}} 'int' lvalue ParmVar {{.*}} 'n' 'int'
  // CHECK-NEXT: InteropTypeExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: PointerType {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: BuiltinType {{.*}} 'int'
  // CHECK-NEXT: ParmVarDecl {{.*}} n 'int'

void f3(int *a, int n) {
  // CHECK: FunctionDecl {{.*}} f3 'void (int * : count(arg #1) itype(_Array_ptr<int>), int)'
  // CHECK-NEXT: ParmVarDecl {{.*}} a 'int *'
  // CHECK-NEXT: CountBoundsExpr
  // CHECK-NEXT: ImplicitCastExpr
  // CHECK-NEXT: DeclRefExpr {{.*}} 'int' lvalue ParmVar {{.*}} 'n' 'int'
  // CHECK-NEXT: InteropTypeExpr {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: PointerType {{.*}} '_Array_ptr<int>'
  // CHECK-NEXT: BuiltinType {{.*}} 'int'
  // CHECK-NEXT: ParmVarDecl {{.*}} n 'int'
  // CHECK-NEXT: CompoundStmt
}
