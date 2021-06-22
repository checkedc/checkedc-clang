// Tests for 3C.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c  -Xclang -verify -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -addcr %t.checked/boundary_tests.c -- | diff %t.checked/boundary_tests.c -
// expected-no-diagnostics

void do_something(int *a, int b) { *a = b; }
//CHECK: void do_something(_Ptr<int> a, int b) _Checked { *a = b; }

void mut(int *a, int b);
//CHECK: void mut(_Ptr<int> a, int b);

void mut(int *a, int b) { *a += b; }
//CHECK: void mut(_Ptr<int> a, int b) _Checked { *a += b; }

void bad_ctx(void) {
  int *x = (int *)0x8001000;
  mut(x, 1);
}

void good_ctx(void) {
  int u = 0;
  mut(&u, 1);
}
