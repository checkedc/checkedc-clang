//
// These is a regression test case for 
// https://github.com/Microsoft/checkedc-clang/issues/254
//
// This test checks that checked pointers and arrays display
// properly in "aka" (also known as) clauses printed in clang diagnostics.
//
// RUN: %clang -cc1 -verify -fcheckedc-extension %s

typedef _Ptr<int> DefTy;
typedef _Ptr<int _Checked[10]> DefArrTy;

void test(DefArrTy arr); // expected-note {{passing argument to parameter 'arr' here}}

void f(void) {
  int i = 0;
  DefTy p = i; // expected-error {{initializing 'DefTy' (aka '_Ptr<int>') with an expression of incompatible type 'int'}}
  test(i);     // expected-error {{passing 'int' to parameter of incompatible type 'DefArrTy' (aka '_Ptr<int _Checked[10]>')}}
}


