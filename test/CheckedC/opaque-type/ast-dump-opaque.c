// Testing opaque type decclarations and usages in AST
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s

// CHECK: TypeOpaqueDecl
typedef _Opaque void* Foo;
