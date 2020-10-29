// More complex tests for bounds widening of _Nt_array_ptr's.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s | FileCheck %s

// expected-no-diagnostics

#include <limits.h>
#include <stdint.h>

void f1(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";

  if (*(j + i + p))
    if (*(i + j + p - 1 - -4 + -2))
  {}

// CHECK: In function: f1
// CHECK:  [B3]
// CHECK:    1: _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";
// CHECK:    2: *(j + i + p)
// CHECK:  [B2]
// CHECK:    1: *(i + j + p - 1 - -4 + -2)
// CHECK: upper_bound(p) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
}

void f2() {
  _Nt_array_ptr<char> p : bounds(p, p) = "a";

  if (*(p + 0))
    if (*(p + 1 - 1 + -1 - +1 - -3))
  {}

// CHECK: In function: f2
// CHECK:  [B3]
// CHECK:    1: _Nt_array_ptr<char> p : bounds(p, p) = "a";
// CHECK:    2: *(p + 0)
// CHECK:  [B2]
// CHECK:    1: *(p + 1 - 1 + -1 - +1 - -3)
// CHECK: upper_bound(p) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
}

void f3(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + (i * j * 2 + 2 + 1)) = "a";

  if (*(p + (i * j * 2 + 3)))
  {}

// CHECK: In function: f3
// CHECK:  [B2]
// CHECK:    1: _Nt_array_ptr<char> p : bounds(p, p + (i * j * 2 + 2 + 1)) = "a";
// CHECK:    2: *(p + (i * j * 2 + 3))
// CHECK:  [B1]
// CHECK: upper_bound(p) = 1
}
