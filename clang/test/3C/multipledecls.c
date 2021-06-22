// Tests for 3C.
//
// Checks handling of multiple declarations.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -Xclang -verify -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -addcr %t.checked/multipledecls.c -- | diff %t.checked/multipledecls.c -
// expected-no-diagnostics

// Comment from PR #576: It appears that this test wants to test a specific
// sequence of multiple declarations. Even if the system `gmtime` probably has
// an analogous sequence of declarations, we want to control the sequence
// precisely in this test. So we use our own name for this function to avoid any
// confusion with the system `gmtime`.
void my_gmtime(long *q);
void my_gmtime(long *q1 : itype(_Ptr<long>));
void foo() {
  long *p;
  long *p1;
  my_gmtime(p);
  my_gmtime(p1);
}
//CHECK: _Ptr<long> p = ((void *)0);
//CHECK-NEXT: _Ptr<long> p1 = ((void *)0);
