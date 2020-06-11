// Tests for using bounds widening to control "out-of-bounds access" errors.
//
// RUN: %clang_cc1 -verify -verify-ignore-unexpected=note -fcheckedc-extension %s

#include <limits.h>

void f1() {
  // TODO: checkedc-clang issue #845: equality between p and ""
  // needs to be recorded in order to properly validate the bounds of p.
  _Nt_array_ptr<char> p : bounds(p, p) = ""; // expected-warning {{cannot prove declared bounds for 'p' are valid after statement}}

  if (*p)
    if (*(p + 1))
      if (*(p + 3)) // expected-error {{out-of-bounds memory access}}
  {}

  if (*p) {
    p++; // expected-error {{declared bounds for 'p' are invalid after statement}}
    if (*(p + 1)) // expected-error {{out-of-bounds memory access}}
    {}
  }
}
