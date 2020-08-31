 // Tests for datafow analysis for bounds widening in case of multiple dereferences of _Nt_array_ptr's.
// RUN: %clang_cc1 -fdump-widened-bounds -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s | FileCheck %s

void f1() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p && *(p + 1)) {}

// CHECK: In function: f1
// CHECK: [B3]
// CHECK:   2: *p
// CHECK: [B2]
// CHECK:   1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK: [B1]
// CHECK: upper_bound(p) = 2
}

void f2() {
  _Nt_array_ptr<char> p : count(0) = "ab";

  if (*p && *(p + 1) && *(p + 2))
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

  if (*p && *(p + 1)) {
    p = "a";
    if (a) {}
  }

// CHECK: In function: f3
// CHECK: [B4]
// CHECK:   3: *p
// CHECK: [B3]
// CHECK:   1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK: [B2]
// CHECK:   1: p = "a"
// CHECK: upper_bound(p) = 2
// CHECK: [B1]
// CHECK-NOT: upper_bound(p)
}

void f4(_Nt_array_ptr<char> p : count(0)) {
  if (p[0] && p[1]) {}

// CHECK: In function: f4
// CHECK: [B9]
// CHECK:   1: p[0]
// CHECK: [B8]
// CHECK:   1: p[1]
// CHECK: upper_bound(p) = 1
// CHECK: [B7]
// CHECK: upper_bound(p) = 2

  _Nt_array_ptr<char> q : count(0) = "a";
  if (0[q] && 1[q]) {}
// CHECK: [B6]
// CHECK:   2: 0[q]
// CHECK: [B5]
// CHECK:   1: 1[q]
// CHECK: upper_bound(q) = 1
// CHECK: [B4]
// CHECK: upper_bound(q) = 2

  if ((((q[0]))) && (((q[1])))) {}
// CHECK: [B3]
// CHECK:   1: (((q[0])))
// CHECK: [B2]
// CHECK:   1: (((q[1])))
// CHECK: upper_bound(q) = 1
// CHECk: [B1]
// CHECK: upper_bound(q) = 2
}

void f5() {
  char p _Nt_checked[] : count(0) = "abc";

  if (p[0] && p[1] && p[2])
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
// It seems it is not correct!
void f6(int i) {
  char p _Nt_checked[] : bounds(p + i, p)  = "abc";

  if (p[0] && p[1]) {
    i = 0; // expected-error {{inferred bounds for 'p' are unknown after statement}}
    if (p[2]) {}
  }

// CHECK: In function: f6
// CHECK: [B4]
// CHECK:  2: p[0]
// CHECK: [B3]
// CHECK:  1: p[1]
// CHECK: upper_bound(p) = 1
// CHECK: [B2]
// CHECK:  1: i = 0
// CHECK:  2: p[2]
// CHECK: upper_bound(p) = 2
// CHECK : [B1]
// CHECK-NOT: upper_bound(p)
}
void f7(char p _Nt_checked[] : count(0)) {
  if (p[0] && p[1]) {}

// CHECK: In function: f7
// CHECK: [B9]
// CHECK:   1: p[0]
// CHECK: [B8]
// CHECK:   1: p[1]
// CHECK: upper_bound(p) = 1
// CHECK: [B7]
// CHECK: upper_bound(p) = 2

  char q _Nt_checked[] : count(0) = "a";
  if (0[q] && 1[q]) {}
// CHECK: [B6]
// CHECK:   2: 0[q]
// CHECK: [B5]
// CHECK:   1: 1[q]
// CHECK: upper_bound(q) = 1
// CHECK: [B4]
// CHECK: upper_bound(q) = 2

  if ((((q[0]))) && (((q[1])))) {}
// CHECK: [B3]
// CHECK:   1: (((q[0])))
// CHECK: [B2]
// CHECK:   1: (((q[1])))
// CHECK: upper_bound(q) = 1
// CHECK: [B1]
// CHECK: upper_bound(q) = 2
}

void f8() {
  _Nt_array_ptr<char> p : count(2) = "abc";

  if (*p && *(p + 1) && *(p + 2) && *(p + 3))
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

  if (*p && *(p + i) && *(p + i + 1)) {}

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

  if (*(i + p + 1 + 2 + 3) && *(3 + p + i + 4) && *(p + i + 9)) {}

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

  if (*(p + j) && *(p + j + 1)) {
    i = 0; // expected-error {{inferred bounds for 'p' are unknown after statement}}
    if (*(p + j + 2)) {}
  }

// CHECK: In function: f11
// CHECK:  [B8]
// CHECK:    2: *(p + j)
// CHECK:  [B7]
// CHECK:    1: *(p + j + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B6]
// CHECK:    1: i = 0
// CHECK:    2: *(p + j + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B5]
// CHECK-NOT: upper_bound(p)

  if (*(p + j) && *(p + j + 1)) {
    j = 0; // expected-error {{inferred bounds for 'p' are unknown after statement}}
    if (*(p + j + 2)) {}
  }

// CHECK:  [B4]
// CHECK:    1: *(p + j)
// CHECK:  [B3]
// CHECK:    1: *(p + j + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B2]
// CHECK:    1: j = 0
// CHECK:    2: *(p + j + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B1]
// CHECK-NOT: upper_bound(p)
}

void f12(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";

  if (*(((p + i + j))) && *((p) + (i) + (j) + (1)) && *((p + i + j) + 2)) {}

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

  if (p[0] && 1[p] && p[2] && 3[p])
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

  if ((1 + i)[p] && p[i] && (1 + i)[p]) {}

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
  if (*(p - i) && *(p - i + 1)) {}

// CHECK: In function: f15
// CHECK:  [B13]
// CHECK:    2: *(p - i)
// CHECK:  [B12]
// CHECK:    1: *(p - i + 1)
// CHECK:  [B11]
// CHECK-NOT: upper_bound(p)

  _Nt_array_ptr<char> q : count(0) = "a";
  if (*q && *(q - 1))
  {}

// CHECK:  [B10]
// CHECK:    2: *q
// CHECK:  [B9]
// CHECK:    1: *(q - 1)
// CHECK: upper_bound(q) = 1
// CHECK:  [B8]
// CHECK: upper_bound(q) = 1
// CHECK-NOT: upper_bound(q)

  _Nt_array_ptr<char> r : bounds(r, r + +1) = "a";
  if (*(r + +1) && *(r + +2))
  {}

// CHECK:  [B7]
// CHECK:    2: *(r + +1)
// CHECK:  [B6]
// CHECK:    1: *(r + +2)
// CHECK: upper_bound(r) = 1
// CHECK:  [B5]
// CHECK: upper_bound(r) = 2

  _Nt_array_ptr<char> s : bounds(s, s + -1) = "a";
  if (*(s + -1) && *(s) && *(s + +1)) // expected-error {{out-of-bounds memory access}}
  {}

// CHECK:  [B4]
// CHECK:    2: *(s + -1)
// CHECK:  [B3]
// CHECK:    1: *(s)
// CHECK: upper_bound(s) = 1
// CHECK:  [B2]
// CHECK:    1: *(s + +1)
// CHECK: upper_bound(s) = 2
// CHECK:  [B1]
// CHECK: upper_bound(s) = 3
}

void f16(_Nt_array_ptr<char> p : bounds(p, p)) {
  _Nt_array_ptr<char> q : bounds(p, p) = "a";
  _Nt_array_ptr<char> r : bounds(p, p + 1) = "a";

  if (*(p) && *(p + 1))
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

  if (*(p) && *(p + 1))
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

  if (p[0] && p[1])
  {}

// CHECK: In function: f18
// CHECK:  [B13]
// CHECK:    5: p[0]
// CHECK:  [B12]
// CHECK:    1: p[1]
// CHECK:  [B11]
// CHECK: upper_bound(p) = 1

  if (q[0] && q[1] && q[2])
  {}

// CHECK:  [B10]
// CHECK:    1: q[0]
// CHECK:  [B9]
// CHECK:    1: q[1]
// CHECK:  [B8]
// CHECK:    1: q[2]
// CHECK:  [B7]
// CHECK: upper_bound(q) = 1

  if (r[0] && r[1]) 
  {}

// CHECK:  [B6]
// CHECK:    1: r[0]
// CHECK:  [B5]
// CHECK:    1: r[1]
// CHECK: upper_bound(r) = 1
// CHECK:  [B4]
// CHECK: upper_bound(r) = 2

  if (s[0] && s[1])
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

  if (*p && *(p + 1) && *(p + 3) && *(p + 2))
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
  _Nt_array_ptr<char> p : bounds(p, ((((((p + 1))))))) = "a";

  if (*(((p + 1))) && *(((p + 2)))) {}

// CHECK: In function: f20
// CHECK: [B3]
// CHECK:   2: *(((p + 1)))
// CHECK: [B2]
// CHECK:   1: *(((p + 2)))
// CHECK: upper_bound(p) = 1
// CHECK: [B1]
// CHECK: upper_bound(p) = 2
}

void f21(_Nt_array_ptr<char> p : count(i), int i, int flag) {
  if (*(p + i) && *(p + i + 1)) {
    flag ? i++ : i;  // expected-error {{inferred bounds for 'p' are unknown after statement}}

    if (*(p + i + 2))
    {}
  }

// CHECK: In function: f21
// CHECK:  [B7]
// CHECK:    1: *(p + i)
// CHECK:  [B6]
// CHECK:    1: *(p + i + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B5]
// CHECK:    1: flag
// CHECK: upper_bound(p) = 2
// CHECK:  [B4]
// CHECK:    1: i
// CHECK: upper_bound(p) = 2
// CHECK:  [B3]
// CHECK:    1: i++
// CHECK: upper_bound(p) = 2
// CHECK:  [B2]
// CHECK:    2: *(p + i + 2)
// CHECK: upper_bound(p) = 2
// CHECK: [B1]
// CHECK-NOT: upper_bound(p)
//
}

void f22() {
  _Nt_array_ptr<char> p : count(0) = "";

 if (((*p && *(p + 1)) && *(p + 2)) && (*(p + 3) && *(p + 4)))
 {}

// CHECK: In function: f22
// CHECK:  [B6]
// CHECK:    2: *p
// CHECK:  [B5]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B4]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B3]
// CHECK:    1: *(p + 3)
// CHECK: upper_bound(p) = 3
// CHECK:  [B2]
// CHECK:    1: *(p + 4)
// CHECK: upper_bound(p) = 4
// CHECK:  [B1]
// CHECK: upper_bound(p) = 5
 }

void f23() {
  _Nt_array_ptr<char> p : count(0) = "";
  _Nt_array_ptr<char> q : count(0) = "";

 if (((*p && *(p + 1))) && *(p + 2) && (*q && *(q + 1)) && (*(p + 3) && *(p + 4)))
 {}

// CHECK: In function: f23
// CHECK:  [B8]
// CHECK:    3: *p
// CHECK:  [B7]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B6]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B5]
// CHECK:    1: *q
// CHECK: upper_bound(p) = 3
// CHECK:  [B4]
// CHECK:    1: *(q + 1)
// CHECK: upper_bound(p) = 3
// CHECK: upper_bound(q) = 1
// CHECK:  [B3]
// CHECK:    1: *(p + 3)
// CHECK: upper_bound(p) = 3
// CHECK: upper_bound(q) = 2
// CHECK:  [B2]
// CHECK:    1: *(p + 4)
// CHECK: upper_bound(p) = 4
// CHECK: upper_bound(q) = 2
// CHECK:  [B1]
// CHECK: upper_bound(p) = 5
// CHECK: upper_bound(q) = 2
}

void f24() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p || *(p + 1)) // expected-error {{out-of-bounds memory access}}
{}

// CHECK: In function: f24
// CHECK: [B3]
// CHECK:   2: *p
// CHECK: [B2]
// CHECK:   1: *(p + 1)
// CHECK-NOT: upper_bound(p)
// CHECK: [B1]
// CHECK-NOT: upper_bound(p)
}
