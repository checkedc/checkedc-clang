// Tests for Checked C rewriter tool.
//
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone %s -- | %clang_cc1  -verify -fcheckedc-extension -x c -
// expected-no-diagnostics

void do_something(int *a, int b) {
  *a = b;
}
//CHECK: void do_something(_Ptr<int> a, int b) {

void mut(int *a, int b);
//CHECK: void  mut(int *a : itype(_Ptr<int>), int b);

void mut(int *a, int b) {
  *a += b;
}
//CHECK: void  mut(int *a : itype(_Ptr<int>), int b) {

void bad_ctx(void) {
  int *x = (int*)0x8001000;
  mut(x, 1);
}

void good_ctx(void) {
  int u = 0;
  mut(&u, 1);
}
