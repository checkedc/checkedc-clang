// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test parsing errors for incorrect bounds cast expressions.

void f(int *p) {
  _Array_ptr<int> ap = _Assume_bounds_cast<_Array_ptr<int>>(p, 1);  // expected-error {{expected bounds expression}}
  // Clang doesn't differentiate between parsing errors and semantic errors
  // during recovery from errors, so make sure recover works OK for a 
  // type checking error.
  ap = _Assume_bounds_cast<_Array_ptr<int>>(p, count(p));             // expected-error {{invalid argument type 'int *'}}
  ap =  _Assume_bounds_cast<_Array_ptr<int>>(p, bounds(p, p + 1) b);  // expected-error {{expected ')'}} \
                                                                      // expected-note {{to match this '('}}
}
