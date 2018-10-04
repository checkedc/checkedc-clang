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
// We don't print anyting unless the inferred checking
// state is not _Unchecked.
// CHECK-NOT: {{_Checked|_Unchecked|checking-state}}
// CHECK-NEXT: CompoundStmt
// CHECK: _Checked checking-state bounds-and-types
// CHECK: CompoundStmt
// CHECK: _Unchecked
// CHECK: CompoundStmt
// CHECK-NOT: {{_Checked|_Unchecked|checking-state}}

void f2(void) _Checked {}

// CHECK-NEXT: FunctionDecl
// CHECK: f2
// CHECK-NEXT: CompoundStmt
// CHECK: _Checked checking-state bounds-and-types

void f3(void) _Unchecked {}

// CHECK-NEXT: FunctionDecl
// CHECK: f3
// CHECK-NEXT: CompoundStmt
// CHECK: _Unchecked
// CHECK-NOT {{checking-state}}

_Checked void f4(void) {}

// CHECK-NEXT: FunctionDecl
// CHECK: f4
// CHECK-NEXT: CompoundStmt
// CHECK: checking-state bounds-and-types

_Unchecked void f5(void) {}

// CHECK-NEXT: FunctionDecl
// CHECK: f5
// CHECK-NEXT: CompoundStmt
// CHECK-NOT: {{_Checked|_Unchecked|checking-state}}
