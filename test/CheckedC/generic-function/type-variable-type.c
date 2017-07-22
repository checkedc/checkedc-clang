// Test to make sure the AST is correctly generated for generic types.
//
// More specifically, test the AST dump of generic function declaration and 
// generic function types associated to it. Generic types are bound to 
// TypedefDecl, which stores the identifier name for the generic type. Also,
// the TypeVariable is is stored inside of FunctionDecl, and should be dumped
// underneath FunctionDecl, before ParmVarDecl.
//
// Also, the test sees if TypeVariable types are correctly displaying for
// variables that has the type TypeVariable.
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s

_For_any(T, S, R) T foo(T test, S steve, R ray) {
  // CHECK: TypedefDecl {{0x[0-9a-f]+}} <{{.+}}.c:{{[0-9]+}}:{{[0-9]+}}, col:[[TPOS:[0-9]+]]> col:[[TPOS]] referenced T '(0, 0)'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
  // CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[SPOS:[0-9]+]]> col:[[SPOS]] referenced S '(0, 1)'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 1)'
  // CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[RPOS:[0-9]+]]> col:[[RPOS]] referenced R '(0, 2)'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 2)'
  // CHECK-NEXT: FunctionDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, line:{{[0-9]+}}:{{[0-9]+}}> line:{{[0-9]+}}:{{[0-9]+}} used foo '_For_any(3) T (T, S, R)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} col:[[TPOS]] T '(0, 0)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} col:[[SPOS]] S '(0, 1)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} col:[[RPOS]] R '(0, 2)'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[TESTPOS:[0-9]+]]> col:[[TESTPOS]] test 'T':'(0, 0)'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[STEVEPOS:[0-9]+]]> col:[[STEVEPOS]] steve 'S':'(0, 1)'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[RAYPOS:[0-9]+]]> col:[[RAYPOS]] ray 'R':'(0, 2)'
  T returnVal;
  return returnVal;
  // CHECK: DeclStmt {{0x[0-9a-f]+}} <line:{{[0-9]+}}:{{[0-9]+}}, col:{{[0-9]+}}>
  // CHECK-NEXT: VarDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:{{[0-9]+}}> col:{{[0-9]+}} used returnVal 'T':'(0, 0)'
  // CHECK-NEXT: ReturnStmt {{0x[0-9a-f]+}} <line:{{[0-9]+}}:{{[0-9]+}}, col:{{[0-9]+}}>
  // CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'T':'(0, 0)'
  // CHECK-NEXT: DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'T':'(0, 0)' lvalue Var {{0x[0-9a-f]+}} 'returnVal' 'T':'(0, 0)'
}

void callPolymorphicTypes() {
  void *t, *s, *r;
  foo<void*, void*, void*>(t, s, r);
}