// Tests for 3C.
//
// Checks for conversions involving const-qualified types.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c  -Xclang -verify -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -addcr %t.checked/const_tests.c -- | diff %t.checked/const_tests.c -
// expected-no-diagnostics

void cst1(const int *a) { int b = *a; }
//CHECK: void cst1(_Ptr<const int> a) _Checked { int b = *a; }

void cst2(int *const a) { *a = 0; }
//CHECK: void cst2(const _Ptr<int> a) _Checked { *a = 0; }

void cst3(const int *a, int i) { int c = *(a + i); }
//CHECK: void cst3(const int *a : itype(_Ptr<const int>), int i) { int c = *(a + i); }

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
  const int *a;
  int b;
} A;
//CHECK: typedef struct _A {
//CHECK-NEXT: _Ptr<const int> a;

void cst5(void) {
  A a = {0};
  int b = 0;
  a.a = &b;
}
