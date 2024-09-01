//
// These is a regression test case for
// https://github.com/checkedc/checkedc-llvm-project/issues/1220
//
// This test checks that the compiler does not crash when it sees the GCC
// statement expression language extension.

// RUN: %clang -cc1 -verify -fcheckedc-extension %s

void f(void) {
  int *a = ({ 0; });  // expected-warning {{ integer to pointer conversion }}
}

