// Tests for datafow analysis for bounds widening of _Nt_array_ptr's.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify -verify-ignore-unexpected=note %s 2>&1 | FileCheck %s

void f1() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p) {}

// CHECK: In function: f1
// CHECK: [B2]
// CHECK:   2: *p
// CHECK: [B1]
// CHECK: upper_bound(p) = 1
}

void f2() {
  _Nt_array_ptr<char> p : count(0) = "ab";

  if (*p)
    if (*(p + 1))   // expected-error {{out-of-bounds memory access}}
      if (*(p + 2)) // expected-error {{out-of-bounds memory access}}
  {}

// CHECK: In function: f2
// CHECK: [B4]
// CHECK:   2: *p
// CHECK: [B3]
// CHECK:   1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK: [B2]
// CHECK:   1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK: [B1]
// CHECK: upper_bound(p) = 3
}

void f3() {
  _Nt_array_ptr<char> p : count(0) = "a";
  int a;

  if (*p) {
    p = "a";
    if (a) {}
  }

// CHECK: In function: f3
// CHECK: [B3]
// CHECK:   3: *p
// CHECK: [B2]
// CHECK:   1: p = "a"
// CHECK: upper_bound(p) = 1
// CHECK: [B1]
// CHECK-NOT: upper_bound(p) = 1
}

void f4(_Nt_array_ptr<char> p : count(0)) {
  if (p[0]) {}

// CHECK: In function: f4
// CHECK: [B4]
// CHECK:   1: p[0]
// CHECK: [B3]
// CHECK:upper_bound(p) = 1

  _Nt_array_ptr<char> q : count(0) = "a";
  if (0[q]) {}
// CHECK: [B2]
// CHECK:   2: 0[q]
// CHECK: [B1]
// CHECK:upper_bound(q) = 1
}

void f5(char p _Nt_checked[] : count(0)) {
  if (p[0]) {}

// CHECK: In function: f5
// CHECK: [B4]
// CHECK:   1: p[0]
// CHECK: [B3]
// CHECK:upper_bound(p) = 1

  char q _Nt_checked[] : count(0) = "a";
  if (0[q]) {}
// CHECK: [B2]
// CHECK:   2: 0[q]
// CHECK: [B1]
// CHECK:upper_bound(q) = 1
}
