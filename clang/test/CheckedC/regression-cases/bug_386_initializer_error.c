//
// This is a regression test case for 
// https://github.com/Microsoft/checkedc-clang/issues/386
//
// The test checks that there are not duplicate error messages when an
// initializer has a syntax or typechecking error.  The compiler was issuing
// a message about a checked variable needed an initializer in that case.
//
// RUN: %clang -cc1 -verify -fcheckedc-extension %s

void f(void) {
  int i = 0;
  _Array_ptr<int> p : count(1) = i + var;  // expected-error {{use of undeclared identifier 'var'}}
}