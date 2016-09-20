// Tests for Checked C rewriter tool.
//
// Checks properties of functions.
//
// RUN: checked-c-convert %s -- | FileCheck %s
// RUN: checked-c-convert %s -- | %clang_cc1 -verify -fcheckedc-extension -x c -
// expected-no-diagnostics

// Have something so that we always get some output.
void a0(void) {
  int q = 0;
  int *k = &q;
  *k = 0;
}
//CHECK: int q = 0;
//CHECK-NEXT: _Ptr<int> k = &q;

void mut(int *);

void a1(void) {
  int a = 0;
  int *b = &a;

  mut(b);
}
//CHECK: int a = 0;
//CHECK-NEXT: int *b = &a;

// Test function pointer assignment and constraint propagation. 

void *xyzzy(int *a, int b) {
  *a = b;

  return 0;
}
//CHECK: void *xyzzy(_Ptr<int> a, int b) {
//CHECK-NEXT: *a = b;

void xyzzy_driver(void) {
  void *(*xyzzy_ptr)(int*, int) = &xyzzy;
  int u = 0;
  int *v = &u;

  xyzzy_ptr(v, u);
}
//CHECK: void xyzzy_driver(void) {
//CHECK-NEXT _Ptr<void* ()(_Ptr<int> , int )> xyzzy_ptr  = &xyzzy;
//CHECK-NEXT int u = 0;
//CHECK-NEXT _Ptr<int> v = &u;

// Test function-like macros.

