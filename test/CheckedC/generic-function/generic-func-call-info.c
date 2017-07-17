// Test to make sure that the generic function calls are correctly instantiated
//
// In CallExpr, the type of the DeclRefExpr should be completely instantiated,
// by replacing all type variable types with specified type names in the list
// of type names provided when calling generic function.
//
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s


_For_any(T, S, R) T foo(T test, S steve, R ray) {
  return test;
}

void callPolymorphicTypes(void) {
  signed int t;
  void *s;
  char *r;
  foo<signed int, void *, char *>(t, s, r);
}

// CHECK: CallExpr {{0x[0-9a-f]+}} <line:{{[0-9]+}}:{{[0-9]+}}, col:{{[0-9]+}}> 'int'
// CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'int (*)(int, void *, char *)' <FunctionToPointerDecay>
// CHECK-NEXT: DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'int (int, void *, char *)' instantiated Function {{0x[0-9a-f]+}} 'foo' '_For_any(3) T (T, S, R)'
// CHECK-NEXT: BuiltinType {{0x[0-9a-f]+}} 'int'
// CHECK-NEXT: PointerType {{0x[0-9a-f]+}} 'void *'
// CHECK-NEXT: BuiltinType {{0x[0-9a-f]+}} 'void'
// CHECK-NEXT: PointerType {{0x[0-9a-f]+}} 'char *'
// CHECK-NEXT: BuiltinType {{0x[0-9a-f]+}} 'char'