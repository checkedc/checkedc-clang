// Tests for bounds widening of _Nt_array_ptr's using function to semantically
// compare two expressions.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s 2>&1 | FileCheck %s

// expected-no-diagnostics

void f1(int i) {
  _Nt_array_ptr<char> p : bounds(p, p + i) = "a";

  if (*(i + p)) {}

// CHECK: In function: f1
// CHECK:   2: *(i + p)
// CHECK: upper_bound(p) = 1
}

void f2(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + (i + j)) = "a";

  if (*(p + (j + i))) {}

// CHECK: In function: f2
// CHECK:   2: *(p + (j + i))
// CHECK: upper_bound(p) = 1
}

void f3(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + (i * j)) = "a";

  if (*(p + (j * i))) {}

// CHECK: In function: f3
// CHECK:   2: *(p + (j * i))
// CHECK: upper_bound(p) = 1
}

void f4(int i, int j, int k, int m, int n) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j + k + m + n) = "a";

  if (*(n + m + k + j + i + p)) {}

// CHECK: In function: f4
// CHECK:   2: *(n + m + k + j + i + p)
// CHECK: upper_bound(p) = 1
}

void f5(int i, int j, int k, int m, int n) {
  _Nt_array_ptr<char> p : bounds(p, (p + i) + (j + k) + (m + n)) = "a";

  if (*((n + m + k) + (j + i + p))) {}

// CHECK: In function: f5
// CHECK:   2: *((n + m + k) + (j + i + p))
// CHECK: upper_bound(p) = 1
}

void f6(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j + i + j) = "a";

  if (*(j + j + p + i + i)) {}

// CHECK: In function: f6
// CHECK:   2: *(j + j + p + i + i)
// CHECK: upper_bound(p) = 1
}

void f7(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i * j) = "a";

  if (*(p + i + j)) {}

// CHECK: In function: f7
// CHECK:   2: *(p + i + j)
// CHECK-NOT: upper_bound(p)
}

void f8(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";

  if (*(p + i + i)) {}

// CHECK: In function: f8
// CHECK:   2: *(p + i + i)
// CHECK-NOT: upper_bound(p)
}

void f9(int i, int j, int k) {
  _Nt_array_ptr<char> p : bounds(p, (p + i) + (j * k)) = "a";

  if (*((p + i) + (j * k))) {}

// CHECK: In function: f9
// CHECK:   2: *((p + i) + (j * k))
// CHECK: upper_bound(p) = 1
}

void f10(int i, int j, int k) {
  _Nt_array_ptr<char> p : bounds(p, (p + i) + (j * k)) = "a";

  if (*((p + i) + (j + k))) {}

// CHECK: In function: f10
// CHECK:   2: *((p + i) + (j + k))
// CHECK-NOT: upper_bound(p)
}

void f11(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";

  if (*(j + i + p))
    if (*(i + j + p - 1 - -4 + -2))
  {}

// CHECK: In function: f11
// CHECK:  [B3]
// CHECK:    1: _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";
// CHECK:    2: *(j + i + p)
// CHECK:  [B2]
// CHECK:    1: *(i + j + p - 1 - -4 + -2)
// CHECK: upper_bound(p) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
}

void f12() {
  _Nt_array_ptr<char> p : bounds(p, p) = "a";

  if (*(p + 0))
    if (*(p + 1 - 1 + -1 - +1 - -3))
  {}

// CHECK: In function: f12
// CHECK:  [B3]
// CHECK:    1: _Nt_array_ptr<char> p : bounds(p, p) = "a";
// CHECK:    2: *(p + 0)
// CHECK:  [B2]
// CHECK:    1: *(p + 1 - 1 + -1 - +1 - -3)
// CHECK: upper_bound(p) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
}

void f13(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + (i * j * 2 + 2 + 1)) = "a";

  if (*(p + (i * j * 2 + 3)))
    if (*(p + (i * j * 2 + 1 + 1 + 1) + 1))
  {}

// CHECK: In function: f13
// CHECK:  [B3]
// CHECK:    1: _Nt_array_ptr<char> p : bounds(p, p + (i * j * 2 + 2 + 1)) = "a";
// CHECK:    2: *(p + (i * j * 2 + 3))
// CHECK:  [B2]
// CHECK:    1: *(p + (i * j * 2 + 1 + 1 + 1) + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
}

void f14(int i) {
  _Nt_array_ptr<char> p : bounds(p, p + (i * 1)) = "a";

  if (*(p + (i * 2)))
  {}

// CHECK: In function: f14
// CHECK:  [B1]
// CHECK-NOT: upper_bound(p)
}

void f15(_Nt_array_ptr<char> p : count(6)) {
  if (*(p + (1 * (2 * 3))))
  {}

// CHECK: In function: f15
//  [B2]
//    1: _Nt_array_ptr<char> p : count(6) = "a";
//    2: *(p + (1 * (2 * 3)))
//  [B1]
// upper_bound(p) = 1
}
