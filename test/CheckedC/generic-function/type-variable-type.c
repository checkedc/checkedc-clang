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

_For_any(T, S, R) _Ptr<T> foo(_Ptr<T> test, _Ptr<S> steve, _Ptr<R> ray) {
  // CHECK: TypedefDecl {{0x[0-9a-f]+}} <{{.+}}.c:{{[0-9]+}}:{{[0-9]+}}, col:[[TPOS:[0-9]+]]> col:[[TPOS]] referenced T '(0, 0)'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
  // CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[SPOS:[0-9]+]]> col:[[SPOS]] referenced S '(0, 1)'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 1)'
  // CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[RPOS:[0-9]+]]> col:[[RPOS]] referenced R '(0, 2)'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 2)'
  // CHECK-NEXT: FunctionDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, line:{{[0-9]+}}:{{[0-9]+}}> line:{{[0-9]+}}:{{[0-9]+}} used foo '_For_any(3) _Ptr<T> (_Ptr<T>, _Ptr<S>, _Ptr<R>)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} col:[[TPOS]] T '(0, 0)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} col:[[SPOS]] S '(0, 1)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} col:[[RPOS]] R '(0, 2)'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[TESTPOS:[0-9]+]]> col:[[TESTPOS]] used test '_Ptr<T>'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[STEVEPOS:[0-9]+]]> col:[[STEVEPOS]] steve '_Ptr<S>'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:[[RAYPOS:[0-9]+]]> col:[[RAYPOS]] ray '_Ptr<R>'
  return test;
  // CHECK: ReturnStmt {{0x[0-9a-f]+}} <line:{{[0-9]+}}:{{[0-9]+}}, col:{{[0-9]+}}>
  // CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Ptr<T>' <LValueToRValue>
  // CHECK-NEXT: DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Ptr<T>' lvalue ParmVar {{0x[0-9a-f]+}} 'test' '_Ptr<T>'
}

void callPolymorphicTypes() {
  int i = 0;
  _Ptr<int> ip = &i;
  foo<int, int, int>(ip, ip, ip);
}