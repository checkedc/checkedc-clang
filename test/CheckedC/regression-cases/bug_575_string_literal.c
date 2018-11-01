//
// These is a regression test case for 
// https://github.com/Microsoft/checkedc-clang/issues/575
//
// This test checks that we do not get duplicate error message for uses
// of arithmetic on checked void pointers and checked function
// pointers in bounds expressions.

// RUN: %clang -cc1 -verify -fcheckedc-extension %s
// expected-no-diagnostics

int main(void) {
     _Nt_array_ptr<const char> str_with_len : count(2) = _Dynamic_bounds_cast<_Nt_array_ptr<const char>>(("\n"), count(2));
    return 0;
}