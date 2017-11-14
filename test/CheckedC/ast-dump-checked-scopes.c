// Tests for dumping of checked/unchecked scopes in ASTs.
// This makes sure that additional information appears as
// expected.
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s

void f1(void) {
  _Checked { int a = 0;  }
  _Unchecked {}
  {}
}

// CHECK: FunctionDecl
// CHECK: f1
// CHECK-NEXT: CompoundStmt
// For inherited-unchecked, we don't print anything to avoid breaking
// existing tests.
// CHECK-NOT: {{.*-checked}}
// CHECK-NEXT: CompoundStmt
// CHECK: declared-checked
// CHECK: CompoundStmt
// CHECK: declared-unchecked
// CHECK: CompoundStmt
// For inherited-unchecked, we don't print anything to avoid breaking
// existing tests.
// CHECK-NOT: {{.*-checked}}

void f2(void) _Checked {}

// CHECK-NEXT: FunctionDecl
// CHECK: f2
// CHECK-NEXT: CompoundStmt
// CHECK: declared-checked

void f3(void) _Unchecked {}

// CHECK-NEXT: FunctionDecl
// CHECK: f3
// CHECK-NEXT: CompoundStmt
// CHECK: declared-unchecked

_Checked void f4(void) {}

// CHECK-NEXT: FunctionDecl
// CHECK: f4
// CHECK-NEXT: CompoundStmt
// CHECK: inherited-checked

_Unchecked void f5(void) {}

// CHECK-NEXT: FunctionDecl
// CHECK: f5
// CHECK-NEXT: CompoundStmt
// For inherited-unchecked, we don't print anything to avoid breaking
// existing tests.
// CHECK-NOT: {{.*-checked}}
