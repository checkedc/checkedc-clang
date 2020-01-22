#include <limits.h>

// RUN: export INT_MAX

// RUN: %clang_cc1 -fdump-widened-bounds -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s 2>&1 | \
// RUN: FileCheck %s -DMAX_INT=%INT_MAX

void f() {
  _Nt_array_ptr<char> p : count(INT_MAX) = "";      // expected-error {{declared bounds for 'p' are invalid after initialization}}

// CHECK: [[MAX_INT]]

// C HECK: 2147483647
}
