// This is a regression test case for 
// https://github.com/Microsoft/checkedc-clang/issues/575
//
// This test checks that the compiler does not crash on a dynamic bounds
// cast whose argument is a string literal.
//
// RUN: %clang -cc1 -verify -fcheckedc-extension %s
// expected-no-diagnostics

int main(void) {
  _Nt_array_ptr<const char> str_with_len : count(2) = 
    _Dynamic_bounds_cast<_Nt_array_ptr<const char>>(("\n"), count(2));
  return 0;
}
