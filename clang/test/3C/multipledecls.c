// Tests for 3C.
//
// Checks handling of multiple declarations.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang_cc1 -verify -fcheckedc-extension -x c -
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -addcr %t.checked/multipledecls.c -- | diff %t.checked/multipledecls.c -
// expected-no-diagnostics

void gmtime(long *q);
void gmtime(long *q1 : itype(_Ptr<long>));
void foo() {
  long *p;
  long *p1;
  gmtime(p);
  gmtime(p1);
}
//CHECK: _Ptr<long> p = ((void *)0);
//CHECK-NEXT: _Ptr<long> p1 = ((void *)0);
