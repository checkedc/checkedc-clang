// Test to make sure that the generic function calls are correctly instantiated
//
// In CallExpr, the type of the DeclRefExpr should be completely instantiated,
// by replacing all type variable types with specified type names in the list
// of type names provided when calling generic function.
//
// Also of important is to check that the resulting LLVM IR correctly shows
// the representation of pointer to type variables. Type variables are treated
// similar to void, as an incomplete type, so the representation of pointer to
// type variables should be the same as void* (i8*).
//
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s --check-prefix=CHECK-AST
// RUN: %clang_cc1 -fcheckedc-extension %s -emit-llvm -O0 -o - | FileCheck %s --check-prefix=CHECK-IR

_For_any(T) _Ptr<T> ptrGenericTest(_Ptr<T> test, int num) {
  return test;
}

// Check the LLVM IR types of the paramters are correct.
// CHECK-IR: define {{.*}}i8* @ptrGenericTest(i8* %test, i32 %num) #0 {

_For_any(T) void arrayPtrGenericTest(_Array_ptr<T> test, int num) {
  return;
}

// Check the LLVM IR types of the paramters are correct.
// CHECK-IR: define {{.*}}void @arrayPtrGenericTest(i8* %test, i32 %num) #0 {

_For_any(T) int funcPtrGenericTest(_Ptr<T> a, _Ptr<T> b, int (*comparator)(_Ptr<T>, _Ptr<T>)) {
  return 0;
}

// Check the LLVM IR types of the paramters are correct.
// CHECK-IR: define {{.*}}i32 @funcPtrGenericTest(i8* %a, i8* %b, i32 (i8*, i8*)* %comparator) #0 {

int compareFunction(_Ptr<int> a, _Ptr<int> b) {
  return 0;
}

void callPolymorphicTypes(void) {
  int t = 0;
  void *s;
  char *r;

  _Ptr<int> pt = &t;
  ptrGenericTest<int>(pt, t);

  // Check the type of DeclRefExpr is correctly instantiated.
  // CHECK-AST: CHKCBindTemporaryExpr {{0x[0-9a-f]+}} <line:{{[0-9]+}}:{{[0-9]+}}, col:{{[0-9]+}}> '_Ptr<int>'
  // CHECK-AST-NEXT: CallExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<int>'
  // CHECK-AST-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Ptr<int> (*)(_Ptr<int>, int)' <FunctionToPointerDecay>
  // CHECK-AST-NEXT: -DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> '_Ptr<int> (_Ptr<int>, int)' instantiated Function {{0x[0-9a-f]+}}  'ptrGenericTest' '_For_any(1) _Ptr<T> (_Ptr<T>, int)'
  // CHECK-AST-NEXT: -BuiltinType {{0x[0-9a-f]+}} 'int'

  _Array_ptr<int> a : count(5) = 0;
  arrayPtrGenericTest<int>(a, 5);

  // Check the type of DeclRefExpr is correctly instantiated.
  // CHECK-AST: CallExpr {{0x[0-9a-f]+}} <line:{{[0-9]+}}:{{[0-9]+}}, col:{{[0-9]+}}> 'void'
  // CHECK-AST-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'void (*)(_Array_ptr<int>, int)' <FunctionToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'void (_Array_ptr<int>, int)' instantiated Function {{0x[0-9a-f]+}} 'arrayPtrGenericTest' '_For_any(1) void (_Array_ptr<T>, int)'

  int result = funcPtrGenericTest<int>(pt, pt, &compareFunction);

  // Check the type of DeclRefExpr is correctly instantiated.
  // CHECK-AST: CallExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}, col:{{[0-9]+}}> 'int'
  // CHECK-AST-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'int (*)(_Ptr<int>, _Ptr<int>, int (*)(_Ptr<int>, _Ptr<int>))' <FunctionToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{0x[0-9a-f]+}} <col:{{[0-9]+}}> 'int (_Ptr<int>, _Ptr<int>, int (*)(_Ptr<int>, _Ptr<int>))' instantiated Function {{0x[0-9a-f]+}} 'funcPtrGenericTest' '_For_any(1) int (_Ptr<T>, _Ptr<T>, int (*)(_Ptr<T>, _Ptr<T>))'
  // CHECK-AST-NEXT: BuiltinType {{0x[0-9a-f]+}} 'int'
}
