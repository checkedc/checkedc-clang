// Tests for using bounds widening to control "out-of-bounds access" errors.
//
// RUN: %clang_cc1 -verify -verify-ignore-unexpected=note -fcheckedc-extension %s

#include <limits.h>

void f1() {
  _Nt_array_ptr<char> p : bounds(p, p) = "";

  if (*p)
    if (*(p + 1))
      if (*(p + 3)) // expected-error {{out-of-bounds memory access}}
  {}

  if (*p) {
    // TODO: checkedc-clang issue #872: declared bounds for p should not be invalid
    p++; // expected-error {{declared bounds for 'p' are invalid after statement}}
    if (*(p + 1)) // expected-error {{out-of-bounds memory access}}
    {}
  }
}
