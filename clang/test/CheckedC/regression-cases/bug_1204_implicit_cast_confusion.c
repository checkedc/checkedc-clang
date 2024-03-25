// This is a regression test case for
//  https://github.com/checkedc/checkedc-llvm-project/issues/1204
//
// This checks that the computation of equivalent expression does
// not accidentally modify implicit casts in the IR.
//
// RUN: %clang -cc1 -verify -fcheckedc-extension %s
// expected-no-diagnostics
//

void f(short int x) {
  unsigned short int u = x;
  u += 1;
}