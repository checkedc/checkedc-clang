// Test to make sure that the generic function calls are correctly instantiated
//
// In CallExpr, the type of the DeclRefExpr should be completely instantiated,
// by replacing all type variable types with specified type names in the list
// of type names provided when calling generic function.
//
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s


_For_any(T, S, R) T genericTest(T test, S steve, R ray) {
  return test;
}

_For_any(T) _Ptr<T> ptrGenericTest(_Ptr<T> test, int num) {
  return test;
}

_For_any(T) void arrayPtrGenericTest(_Array_ptr<T> test, int num) {
  return;
}

_For_any(T) int funcPtrGenericTest(_Ptr<T> a, _Ptr<T> b, int (*comparator)(_Ptr<T>, _Ptr<T>)) {
  return 0;
}

int compareFunction(_Ptr<int> a, _Ptr<int> b) {
  return 0;
}

void callPolymorphicTypes(void) {
  int t = 0;
  void *s;
  char *r;
  genericTest<signed int, void *, char *>(t, s, r);

  // CHECK: CallExpr {{0x[0-9a-f]+}} <line:{{[0-9]+}}:{{[0-9]+}}, col:{{[0-9]+}}> 'int'
  // CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'int (*)(int, void *, char *)' <FunctionToPointerDecay>
  // CHECK-NEXT: DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'int (int, void *, char *)' instantiated Function {{0x[0-9a-f]+}} 'genericTest' '_For_any(3) T (T, S, R)'
  // CHECK-NEXT: BuiltinType {{0x[0-9a-f]+}} 'int'
  // CHECK-NEXT: PointerType {{0x[0-9a-f]+}} 'void *'
  // CHECK-NEXT: BuiltinType {{0x[0-9a-f]+}} 'void'
  // CHECK-NEXT: PointerType {{0x[0-9a-f]+}} 'char *'
  // CHECK-NEXT: BuiltinType {{0x[0-9a-f]+}} 'char'

  _Ptr<int> pt = &t;
  ptrGenericTest<int>(pt, t);

  // CHECK: CallExpr {{0x[0-9a-f]+}} <line:{{[0-9]+}}:{{[0-9]+}}, col:{{[0-9]+}}> '_Ptr<int>'
  // CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Ptr<int> (*)(_Ptr<int>, int)' <FunctionToPointerDecay>
  // CHECK-NEXT: -DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Ptr<int> (_Ptr<int>, int)' instantiated Function {{0x[0-9a-f]+}}  'ptrGenericTest' '_For_any(1) _Ptr<T> (_Ptr<T>, int)'
  // CHECK-NEXT: -BuiltinType {{0x[0-9a-f]+}} 'int'

  _Array_ptr<int> a : count(5) = 0;
  arrayPtrGenericTest<int>(a, 5);

  // CHECK: CallExpr {{0x[0-9a-f]+}} <line:{{[0-9]+}}:{{[0-9]+}}, col:{{[0-9]+}}> 'void'
  // CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'void (*)(_Array_ptr<int>, int)' <FunctionToPointerDecay>
  // CHECK-NEXT: DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'void (_Array_ptr<int>, int)' instantiated Function {{0x[0-9a-f]+}} 'arrayPtrGenericTest' '_For_any(1) void (_Array_ptr<T>, int)'

  int result = funcPtrGenericTest<int>(pt, pt, &compareFunction);

  // CHECK: -CallExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:{{[0-9]+}}> 'int'
  // CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'int (*)(_Ptr<int>, _Ptr<int>, int (*)(_Ptr<int>, _Ptr<int>))' <FunctionToPointerDecay>
  // CHECK-NEXT: DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'int (_Ptr<int>, _Ptr<int>, int (*)(_Ptr<int>, _Ptr<int>))' instantiated Function {{0x[0-9a-f]+}} 'funcPtrGenericTest' '_For_any(1) int (_Ptr<T>, _Ptr<T>, int (*)(_Ptr<T>, _Ptr<T>))'
  // CHECK-NEXT: BuiltinType {{0x[0-9a-f]+}} 'int'
}
