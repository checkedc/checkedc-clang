// Tests for Checked C rewriter tool.
//
// Checks handling of multiple declarations.
//
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone %s -- | %clang_cc1 -verify -fcheckedc-extension -x c -
// expected-no-diagnostics

void gmtime(long *q);
void gmtime(long *q1:itype(_Ptr<long>));
void foo() {
  long *p;
  long *p1;
  gmtime(p);
  gmtime(p1);
}
//CHECK: _Ptr<long> p = ((void *)0);
//CHECK-NEXT: _Ptr<long> p1 = ((void *)0);
