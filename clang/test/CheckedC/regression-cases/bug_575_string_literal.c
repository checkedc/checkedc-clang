// This is a regression test case for 
// https://github.com/Microsoft/checkedc-clang/issues/575
//
// This test checks that the compiler does not crash on a dynamic bounds
// cast whose argument is a string literal.
//
// RUN: %clang -cc1 -verify -fcheckedc-extension %s

int main(void) {
  // TODO: checkedc-clang issue #845: equality between str_with_len and ""
  // needs to be recorded in order to properly check the bounds of str_with_len.
  _Nt_array_ptr<const char> str_with_len : count(2) =  _Dynamic_bounds_cast<_Nt_array_ptr<const char>>((""), count(2)); // expected-warning {{cannot prove declared bounds for 'str_with_len' are valid after statement}} \
                                                                                                                          // expected-note {{(expanded) declared bounds are 'bounds(str_with_len, str_with_len + 2)'}} \
                                                                                                                          // expected-note {{(expanded) inferred bounds are 'bounds((_Nt_array_ptr<const char>)value of (""), (_Nt_array_ptr<const char>)value of ("") + 2)'}}
  return 0;
}
