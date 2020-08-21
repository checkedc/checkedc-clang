// Tests for Checked C rewriter tool.
//
// Checks for conversions involving const-qualified types.
//
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone -addcr %s -- | %clang_cc1  -verify -fcheckedc-extension -x c -
// RUN: cconv-standalone -addcr -output-postfix=checked %s 
// RUN: cconv-standalone -addcr %S/const_tests.checked.c -- | count 0
// RUN: rm %S/const_tests.checked.c
// expected-no-diagnostics

void cst1(const int *a) {
  int b = *a;
}
//CHECK: void cst1(_Ptr<const int> a) _Checked {

void cst2(int * const a) {
  *a = 0;
}
//CHECK: void cst2(const _Ptr<int> a) _Checked {

void cst3(const int *a, int i) {
  int c = *(a+i);
}
//CHECK: void cst3(const int *a, int i) {
//CHECK-NEXT: int c = *(a+i);

void cst4(const int *b) {
  int c = *b;
  const int *d = b;
  int e = *d;
}
//CHECK: void cst4(_Ptr<const int> b) _Checked {
//CHECK-NEXT: int c = *b;
//CHECK-NEXT: _Ptr<const int> d = b;
//CHECK-NEXT: int e = *d;

typedef struct _A { 
  const int * a;
  int b;
} A;
//CHECK: typedef struct _A {
//CHECK-NEXT: _Ptr<const int> a;

void cst5(void) {
  A a = { 0 };
  int b = 0;
  a.a = &b;
}
