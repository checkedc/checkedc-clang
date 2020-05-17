// Regression test for the case where a member base expression is not an
// lvalue.  The Checked C compiler asserted during bounds inference when
// it saw this (Github issue #231).
//
// RUN: %clang_cc1 -verify -fcheckedc-extension %s
// expected-no-diagnostics

struct S {
  int m;
};

struct S f1(void) {
  struct S tmp;
  tmp.m = 0;
  return tmp;
}

int f2(void) {
  int i = f1().m;
  return i;
}

int f3(void) {
  struct S a;
  int i = (a = f1()).m;
  return i;
}