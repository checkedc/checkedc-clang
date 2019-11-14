// Tests for serialization/deserialization of checked scopes.
// This makes sure AST serialization/deserialization accounts for
// checked scope specifiers and written checked scope specifiers.
// 
// RUN: %clang_cc1 -fcheckedc-extension -emit-pch -o %t %S/ast-dump-pch.h
// RUN: %clang_cc1 -fcheckedc-extension -ast-dump-all -include-pch %t %s | FileCheck %s

// CHECK: FunctionDecl
// CHECK: f1
// CHECK-NEXT: CompoundStmt
// CHECK-NOT: {{_Checked|_Unchecked|checking-state}}
// CHECK-NEXT: CompoundStmt
// CHECK: _Checked checking-state bounds-and-types
// CHECK: CompoundStmt
// CHECK: _Checked _Bounds_only checking-state bounds
// CHECK: CompoundStmt
// CHECK: _Unchecked
// CHECK: CompoundStmt
// CHECK-NOT: {{_Checked|_Unchecked|checking-state}}

// CHECK-NEXT: FunctionDecl
// CHECK: f2
// CHECK-NEXT: CompoundStmt
// CHECK: _Checked checking-state bounds-and-types

// CHECK-NEXT: FunctionDecl
// CHECK: f3
// CHECK-NEXT: CompoundStmt
// CHECK: _Checked _Bounds_only checking-state bounds

// CHECK-NEXT: FunctionDecl
// CHECK: f4
// CHECK-NEXT: CompoundStmt
// CHECK: _Unchecked
// CHECK-NOT {{checking-state}}

// CHECK-NEXT: FunctionDecl
// CHECK: f5
// CHECK: checked
// CHECK-NEXT: CompoundStmt
// CHECK: checking-state bounds-and-types

// CHECK-NEXT: FunctionDecl
// CHECK: f6
// CHECK: checked bounds_only
// CHECK-NEXT: CompoundStmt
// CHECK: checking-state bounds

// CHECK-NEXT: FunctionDecl
// CHECK: f7
// CHECK: unchecked
// CHECK-NEXT: CompoundStmt
// CHECK-NOT: {{_Checked|_Unchecked|checking-state}}

// CHECK-NEXT: FunctionDecl
// CHECK: f8
// CHECK: checked
// CHECK-NEXT: CompoundStmt
// CHECK: checking-state bounds