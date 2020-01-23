// Tests for datafow analysis for bounds widening of _Nt_array_ptr's.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s 2>&1 | FileCheck %s

#include <limits.h>

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
// CHECK: [B6]
// CHECK:   1: p[0]
// CHECK: [B5]
// CHECK: upper_bound(p) = 1

  _Nt_array_ptr<char> q : count(0) = "a";
  if (0[q]) {}
// CHECK: [B4]
// CHECK:   2: 0[q]
// CHECK: [B3]
// CHECK: upper_bound(q) = 1

  if ((((q[0])))) {}
// CHECK: [B2]
// CHECK:   1: (((q[0])))
// CHECK: [B1]
// CHECK: upper_bound(q) = 1
}

void f5() {
  char p _Nt_checked[] : count(0) = "abc";

  if (p[0])
    if (p[1])   // expected-error {{out-of-bounds memory access}}
      if (p[2]) // expected-error {{out-of-bounds memory access}}
  {}

// CHECK: In function: f5
// CHECK: [B4]
// CHECK:   2: p[0]
// CHECK: [B3]
// CHECK:   1: p[1]
// CHECK: upper_bound(p) = 1
// CHECK: [B2]
// CHECK:   1: p[2]
// CHECK: upper_bound(p) = 2
// CHECK: [B1]
// CHECK: upper_bound(p) = 3
}

void f6(int i) {
  char p _Nt_checked[] : bounds(p + i, p)  = "abc";

  if (p[0]) {
    i = 0;
    if (p[1]) {}
  }

// CHECK: In function: f6
// CHECK: [B3]
// CHECK:   2: p[0]
// CHECK: [B2]
// CHECK:   1: i = 0
// CHECK:   2: p[1]
// CHECK: upper_bound(p) = 1
// CHECK: [B1]
// CHECK-NOT: upper_bound(p)
}

void f7(char p _Nt_checked[] : count(0)) {
  if (p[0]) {}

// CHECK: In function: f7
// CHECK: [B6]
// CHECK:   1: p[0]
// CHECK: [B5]
// CHECK: upper_bound(p) = 1

  char q _Nt_checked[] : count(0) = "a";
  if (0[q]) {}
// CHECK: [B4]
// CHECK:   2: 0[q]
// CHECK: [B3]
// CHECK: upper_bound(q) = 1

  if ((((q[0])))) {}
// CHECK: [B2]
// CHECK:   1: (((q[0])))
// CHECK: [B1]
// CHECK: upper_bound(q) = 1
}

void f8() {
  _Nt_array_ptr<char> p : count(2) = "abc";

  if (*p)
    if (*(p + 1))
      if (*(p + 2))
        if (*(p + 3)) // expected-error {{out-of-bounds memory access}}
  {}

// CHECK: In function: f8
// CHECK: [B5]
// CHECK:   2: *p
// CHECK: [B4]
// CHECK:   1: *(p + 1)
// CHECK-NOT: upper_bound(p)
// CHECK: [B3]
// CHECK:   1: *(p + 2)
// CHECK-NOT: upper_bound(p)
// CHECK: [B2]
// CHECK:   1: *(p + 3)
// CHECK: upper_bound(p) = 1
// CHECK: [B1]
// CHECK: upper_bound(p) = 2
}

void f9(int i) {
  _Nt_array_ptr<char> p : bounds(p, p + i) = "a";

  if (*p)
    if (*(p + i))
      if (*(p + i + 1)) {}

// CHECK: In function: f9
// CHECK: [B4]
// CHECK:   2: *p
// CHECK: [B3]
// CHECK:   1: *(p + i)
// CHECK-NOT: upper_bound(p)
// CHECK: [B2]
// CHECK:   1: *(p + i + 1)
// CHECK: upper_bound(p) = 1
// CHECK: [B1]
// CHECK: upper_bound(p) = 2
}

void f10(int i) {
  _Nt_array_ptr<char> p : bounds(p, 1 + p + i + 5) = "a";

  if (*(i + p + 1 + 2 + 3))
    if (*(3 + p + i + 4))
      if (*(p + i + 9)) {}

// CHECK: In function: f10
// CHECK:  [B4]
// CHECK:    2: *(i + p + 1 + 2 + 3)
// CHECK:  [B3]
// CHECK:    1: *(3 + p + i + 4)
// CHECK: upper_bound(p) = 1
// CHECK:  [B2]
// CHECK:    1: *(p + i + 9)
// CHECK: upper_bound(p) = 2
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
}

void f11(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p + i, p + j) = "a";

  if (*(p + j)) {
    i = 0;
    if (*(p + j + 1)) {}
  }

// CHECK: In function: f11
// CHECK:  [B6]
// CHECK:    2: *(p + j)
// CHECK:  [B5]
// CHECK:    1: i = 0
// CHECK:    2: *(p + j + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B4]
// CHECK-NOT: upper_bound(p)

  if (*(p + j)) {
    j = 0;
    if (*(p + j + 1)) {}
  }

// CHECK:  [B3]
// CHECK:    1: *(p + j)
// CHECK:    T: if [B3.1]
// CHECK:  [B2]
// CHECK:    1: j = 0
// CHECK:    2: *(p + j + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B1]
// CHECK-NOT: upper_bound(p)
}

void f12(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";

  if (*(((p + i + j))))
    if (*((p) + (i) + (j) + (1)))
      if (*((p + i + j) + 2)) {}

// CHECK: In function: f12
// CHECK:  [B4]
// CHECK:    2: *(((p + i + j)))
// CHECK:  [B3]
// CHECK:    1: *((p) + (i) + (j) + (1))
// CHECK: upper_bound(p) = 1
// CHECK:  [B2]
// CHECK:    1: *((p + i + j) + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B1]
// CHECK: upper_bound(p) = 3
}

void f13() {
  char p _Nt_checked[] : count(1) = "a";

  if (p[0])
    if (1[p])
      if (p[2])   // expected-error {{out-of-bounds memory access}}
        if (3[p]) // expected-error {{out-of-bounds memory access}}
  {}

// CHECK: In function: f13
// CHECK:  [B5]
// CHECK:    2: p[0]
// CHECK:  [B4]
// CHECK:    1: 1[p]
// CHECK-NOT: upper_bound(p)
// CHECK:  [B3]
// CHECK:    1: p[2]
// CHECK: upper_bound(p) = 1
// CHECK:  [B2]
// CHECK:    1: 3[p]
// CHECK: upper_bound(p) = 2
// CHECK:  [B1]
// CHECK: upper_bound(p) = 3
}

void f14(int i) {
  char p _Nt_checked[] : bounds(p, p + i) = "a";

  if ((1 + i)[p])
    if (p[i])
      if ((1 + i)[p]) {}

// CHECK: In function: f14
// CHECK:  [B4]
// CHECK:    2: (1 + i)[p]
// CHECK:  [B3]
// CHECK:    1: p[i]
// CHECK-NOT: upper_bound(p)
// CHECK:  [B2]
// CHECK:    1: (1 + i)[p]
// CHECK: upper_bound(p) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
}

void f15(int i) {
  _Nt_array_ptr<char> p : bounds(p, p - i) = "a";
  if (*(p - i)) {}

// CHECK: In function: f15
// CHECK:  [B9]
// CHECK:    2: *(p - i)
// CHECK:  [B8]
// CHECK-NOT: upper_bound(p)

  _Nt_array_ptr<char> q : count(0) = "a";
  if (*q)
    if (*(q - 1)) // expected-error {{out-of-bounds memory access}}
  {}

// CHECK:  [B7]
// CHECK:    2: *q
// CHECK:  [B6]
// CHECK:    1: *(q - 1)
// CHECK: upper_bound(q) = 1
// CHECK:  [B5]
// CHECK: upper_bound(q) = 1
// CHECK-NOT: upper_bound(q)

  _Nt_array_ptr<char> r : bounds(r, r + +1) = "a";
  if (*(r + +1))
  {}

// CHECK:  [B4]
// CHECK:    2: *(r + +1)
// CHECK:  [B3]
// CHECK: upper_bound(r) = 1

  _Nt_array_ptr<char> s : bounds(s, s + -1) = "a";
  if (*(s + -1)) // expected-error {{out-of-bounds memory access}}
  {}

// CHECK:  [B2]
// CHECK:    2: *(s + -1)
// CHECK:  [B1]
// CHECK: upper_bound(s) = 1
}

void f16(_Nt_array_ptr<char> p : bounds(p, p)) {
  _Nt_array_ptr<char> q : bounds(p, p) = "a";
  _Nt_array_ptr<char> r : bounds(p, p + 1) = "a";

  if (*(p))
    if (*(p + 1)) // expected-error {{out-of-bounds memory access}}
  {}

// CHECK: In function: f16
// CHECK:  [B3]
// CHECK:    3: *(p)
// CHECK:  [B2]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK: upper_bound(q) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
// CHECK: upper_bound(q) = 2
// CHECK: upper_bound(r) = 1
}

void f17(char p _Nt_checked[] : count(1)) {
  _Nt_array_ptr<char> q : bounds(p, p + 1) = "a";
  _Nt_array_ptr<char> r : bounds(p, p) = "a";

  if (*(p))
    if (*(p + 1))
  {}

// CHECK: In function: f17
// CHECK:  [B3]
// CHECK:    3: *(p)
// CHECK:  [B2]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(r) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 1
// CHECK: upper_bound(q) = 1
// CHECK: upper_bound(r) = 2
}

void f18() {
  char p _Nt_checked[] = "a";
  char q _Nt_checked[] = "ab";
  char r _Nt_checked[] : count(0) = "ab";
  char s _Nt_checked[] : count(1) = "ab";

  if (p[0])
    if (p[1])
  {}

// CHECK: In function: f18
// CHECK:  [B12]
// CHECK:    5: p[0]
// CHECK:  [B11]
// CHECK:    1: p[1]
// CHECK:  [B10]
// CHECK: upper_bound(p) = 1

  if (q[0])
    if (q[1])
      if (q[2])
  {}

// CHECK:  [B9]
// CHECK:    1: q[0]
// CHECK:  [B8]
// CHECK:    1: q[1]
// CHECK:  [B7]
// CHECK:    1: q[2]
// CHECK:  [B6]
// CHECK: upper_bound(q) = 1

  if (r[0])
  {}

// CHECK:  [B5]
// CHECK:    1: r[0]
// CHECK:  [B4]
// CHECK: upper_bound(r) = 1

  if (s[0])
    if (s[1])
  {}

// CHECK:  [B3]
// CHECK:    1: s[0]
// CHECK:  [B2]
// CHECK:    1: s[1]
// CHECK:  [B1]
// CHECK: upper_bound(s) = 1
}

void f19() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p)
    if (*(p + 1))     // expected-error {{out-of-bounds memory access}}
      if (*(p + 3))   // expected-error {{out-of-bounds memory access}}
        if (*(p + 2)) // expected-error {{out-of-bounds memory access}}
  {}

// CHECK: In function: f19
// CHECK:  [B5]
// CHECK:    2: *p
// CHECK:  [B4]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B3]
// CHECK:    1: *(p + 3)
// CHECK: upper_bound(p) = 2
// CHECK:  [B2]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B1]
// CHECK: upper_bound(p) = 3
}

void f20() {
  // Declared bounds and deref offset are both INT_MAX. Valid widening.
  _Nt_array_ptr<char> p : count(INT_MAX) = "";      // expected-error {{declared bounds for 'p' are invalid after initialization}}
  if (*(p + INT_MAX))
  {}

// CHECK: In function: f20
// CHECK:  [B12]
// CHECK:    2: *(p + {{.*}})
// CHECK:  [B11]
// CHECK: upper_bound(p) = 1

  // Declared bounds and deref offset are both INT_MIN. Valid widening.
  _Nt_array_ptr<char> q : count(INT_MIN) = "";
  if (*(q + INT_MIN))                               // expected-error {{out-of-bounds memory access}}
  {}

// CHECK:  [B10]
// CHECK:    2: *(q + {{.*}})
// CHECK:  [B9]
// CHECK: upper_bound(q) = 1

  // Declared bounds (INT_MIN) and deref offset (INT_MAX - 1). No sequential deref tests. No widening.
  _Nt_array_ptr<char> r : count(INT_MIN) = "";
  if (*(r + INT_MAX - 1))                               // expected-error {{out-of-bounds memory access}}
  {}

// CHECK:  [B8]
// CHECK:    2: *(r + {{.*}})
// CHECK:  [B7]
// CHECK-NOT: upper_bound(r)

  // Declared bounds and deref offset are both (INT_MAX + 1). Integer overflow. No widening.
  _Nt_array_ptr<char> s : count(INT_MAX + 1) = "";
  if (*(s + INT_MAX + 1))                           // expected-error {{out-of-bounds memory access}}
  {}

// CHECK:  [B6]
// CHECK:    2: *(s + {{.*}})
// CHECK:  [B5]
// CHECK-NOT: upper_bound(s)

  // Declared bounds and deref offset are both (INT_MIN + 1). Valid widening.
  _Nt_array_ptr<char> t : count(INT_MIN + 1) = "";
  if (*(t + INT_MIN + 1))                           // expected-error {{out-of-bounds memory access}}
  {}

// CHECK:  [B4]
// CHECK:    2: *(t + {{.*}})
// CHECK:  [B3]
// CHECK: upper_bound(t) = 1

  // Declared bounds and deref offset are both (INT_MIN + -1). Integer underflow. No widening.
  _Nt_array_ptr<char> u : count(INT_MIN + -1) = ""; // expected-error {{declared bounds for 'u' are invalid after initialization}}
  if (*(u + INT_MIN + -1))
  {}

// CHECK:  [B2]
// CHECK:    2: *(u + {{.*}})
// CHECK:  [B1]
// CHECK-NOT: upper_bound(u)
}
