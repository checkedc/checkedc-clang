// Tests for datafow analysis for bounds widening of null-terminated arrays.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify \
// RUN: -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s \
// RUN: | FileCheck %s

#include <limits.h>
#include <stdint.h>

int a;

void f1() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p) {
    a = 1;
  }
  a = 2;

// CHECK: Function: f1
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)
}

void f2() {
  _Nt_array_ptr<char> p : count(0) = "ab";

  if (*p) {
    a = 1;
    if (*(p + 1)) {
      a = 2;
      if (*(p + 2)) {
        a = 3;
      }
      a = 4;
    }
    a = 5;
  }
  a = 6;

// CHECK: Function: f2
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "ab";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B2, Pred: B3, B6, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B1, Pred: B2, B7, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 0)
}

void f3() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p) {
    p = "a";
    if (a) {
      a = 1;
    }
    a = 2;
  }
  a = 3;

// CHECK: Function: f3
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B4, Pred: B5, Succ: B3, B2
// CHECK:   Widened bounds before stmt: p = "a"
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: a
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B2, Pred: B3, B4, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B2, B5, Succ: B0
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)
}

void f4(_Nt_array_ptr<char> p : count(0)) {
  if (p[0]) {
    a = 1;
  }
  a = 2;

  _Nt_array_ptr<char> q : count(0) = "a";
  if (0[q]) {
    a = 3;
  }
  a = 4;

  if ((((q[0])))) {
    a = 5;
  }
  a = 6;

// CHECK: Function: f4
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B5
// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK: Block: B5, Pred: B6, B7, Succ: B4, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : count(0) = "a";
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: 0[q]
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0 + 1)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK:   Widened bounds before stmt: (((q[0])))
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0 + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)
}

void f5() {
  char p _Nt_checked[] : count(0) = "abc";

  if (p[0]) {
    a = 1;
    if (p[1]) {
      a = 2;
      if (p[2]) {
        a = 3;
      }
      a = 4;
    }
    a = 5;
  }
  a = 6;

// CHECK: Function: f5
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] : count(0) = "abc";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK:   Widened bounds before stmt: p[1]
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK:   Widened bounds before stmt: p[2]
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B2, Pred: B3, B6, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK: Block: B1, Pred: B2, B7, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 0)
}

void f6(int i) {
  char p _Nt_checked[] : bounds(p + i, p)  = "abc";

  if (p[0]) {
    i = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    if (p[1]) {
      a = 1;
    }
    a = 2;
  }
  a = 3;

// CHECK: Function: f6
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] : bounds(p + i, p) = "abc";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p + i, p)

// CHECK: Block: B4, Pred: B5, Succ: B3, B2
// CHECK:   Widened bounds before stmt: i = 0
// CHECK:     p: bounds(p + i, p + 0 + 1)

// CHECK:   Widened bounds before stmt: p[1]
// CHECK:     p: bounds(p + i, p)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p + i, p)

// CHECK: Block: B2, Pred: B3, B4, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p + i, p)
}

void f7(char p _Nt_checked[] : count(0)) {
  if (p[0]) {
    a = 1;
  }
  a = 2;

  char q _Nt_checked[] : count(0) = "a";
  if (0[q]) {
    a = 3;
  }
  a = 4;

  if ((((q[0])))) {
    a = 5;
  }
  a = 6;

// CHECK: Function: f7
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B5
// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK: Block: B5, Pred: B6, B7, Succ: B4, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: char q _Nt_checked[] : count(0) = "a";
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: 0[q]
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0 + 1)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK:   Widened bounds before stmt: (((q[0])))
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0 + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)
}

void f8() {
  _Nt_array_ptr<char> p : count(2) = "abc";

  if (*p) {
    a = 1;
    if (*(p + 1)) {
      a = 2;
      if (*(p + 2)) {
        a = 3;
        if (*(p + 3)) {
          a = 4;
        }
        a = 5;
      }
      a = 6;
    }
    a = 7;
  }
  a = 8;

// CHECK: Function: f8
// CHECK: Block: B10 (Entry), Pred: Succ: B9

// CHECK: Block: B9, Pred: B10, Succ: B8, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(2) = "abc";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 2)

// CHECK: Block: B8, Pred: B9, Succ: B7, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 2)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 2)

// CHECK: Block: B7, Pred: B8, Succ: B6, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 2)

// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 2)

// CHECK: Block: B6, Pred: B7, Succ: B5, B4
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK:   Widened bounds before stmt: *(p + 3)
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 3 + 1)

// CHECK: Block: B4, Pred: B5, B6, Succ: B3
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B3, Pred: B4, B7, Succ: B2
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 2)

// CHECK: Block: B2, Pred: B3, B8, Succ: B1
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 2)

// CHECK: Block: B1, Pred: B2, B9, Succ: B0
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 2)
}

void f9(int i) {
  _Nt_array_ptr<char> p : bounds(p, p + i) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*p) {
    a = 1;
    if (*(p + i)) {
      a = 2;
      if (*(p + i + 1)) {
        a = 3;
      }
      a = 4;
    }
    a = 5;
  }
  a = 6;

// CHECK: Function: f9
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B6, Pred: B7, Succ: B5, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + i)

// CHECK:   Widened bounds before stmt: *(p + i)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B5, Pred: B6, Succ: B4, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + i + 1)

// CHECK:   Widened bounds before stmt: *(p + i + 1)
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + i + 1 + 1)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B2, Pred: B3, B6, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B1, Pred: B2, B7, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + i)
}

void f10(int i) {
  _Nt_array_ptr<char> p : bounds(p, 1 + p + i + 5) = "a";

  if (*(i + p + 1 + 2 + 3)) {
    a = 1;
    if (*(3 + p + i + 4)) {
      a = 2;
      if (*(p + i + 9)) {
        a = 3;
      }
      a = 4;
    }
    a = 5;
  }
  a = 6;

// CHECK: Function: f10
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, 1 + p + i + 5) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(i + p + 1 + 2 + 3)
// CHECK:     p: bounds(p, 1 + p + i + 5)

// CHECK: Block: B6, Pred: B7, Succ: B5, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, i + p + 1 + 2 + 3 + 1)

// CHECK:   Widened bounds before stmt: *(3 + p + i + 4)
// CHECK:     p: bounds(p, i + p + 1 + 2 + 3 + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, 3 + p + i + 4 + 1)

// CHECK:   Widened bounds before stmt: *(p + i + 9)
// CHECK:     p: bounds(p, 3 + p + i + 4 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, 3 + p + i + 4 + 1)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, 3 + p + i + 4 + 1)

// CHECK: Block: B2, Pred: B3, B6, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, i + p + 1 + 2 + 3 + 1)

// CHECK: Block: B1, Pred: B2, B7, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, 1 + p + i + 5)
}

void f11(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p + i, p + j) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(p + j)) {
    i = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    if (*(p + j + 1)) {
      a = 1;
    }
    a = 2;
  }
  a = 3;

// CHECK: Function: f11
// CHECK: Block: B10 (Entry), Pred: Succ: B9

// CHECK: Block: B9, Pred: B10, Succ: B8, B5
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p + i, p + j) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + j)
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B8, Pred: B9, Succ: B7, B6
// CHECK:   Widened bounds before stmt: i = 0
// CHECK:     p: bounds(p + i, p + j + 1)

// CHECK:   Widened bounds before stmt: *(p + j + 1)
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B7, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B6, Pred: B7, B8, Succ: B5
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B5, Pred: B6, B9, Succ: B4, B1
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p + i, p + j)

  if (*(p + j)) {
    j = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    if (*(p + j + 1)) {
      a = 4;
    }
    a = 5;
  }
  a = 6;

// CHECK:   Widened bounds before stmt: *(p + j)
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B4, Pred: B5, Succ: B3, B2
// CHECK:   Widened bounds before stmt: j = 0
// CHECK:     p: bounds(p + i, p + j + 1)

// CHECK:   Widened bounds before stmt: *(p + j + 1)
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B2, Pred: B3, B4, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B1, Pred: B2, B5, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p + i, p + j)
}

void f12(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";

  if (*(((p + i + j)))) {
    a = 1;
    if (*((p) + (i) + (j) + (1))) {
      a = 2;
      if (*((p + i + j) + 2)) {
        a = 3;
      }
      a = 4;
    }
    a = 5;
  }
  a = 6;

// CHECK: Function: f12
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(((p + i + j)))
// CHECK:     p: bounds(p, p + i + j)

// CHECK: Block: B6, Pred: B7, Succ: B5, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + i + j + 1)

// CHECK:   Widened bounds before stmt: *((p) + (i) + (j) + (1))
// CHECK:     p: bounds(p, p + i + j + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, (p) + (i) + (j) + (1) + 1)

// CHECK:   Widened bounds before stmt: *((p + i + j) + 2)
// CHECK:     p: bounds(p, (p) + (i) + (j) + (1) + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, (p + i + j) + 2 + 1)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, (p) + (i) + (j) + (1) + 1)

// CHECK: Block: B2, Pred: B3, B6, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + i + j + 1)

// CHECK: Block: B1, Pred: B2, B7, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + i + j)
}

void f13() {
  char p _Nt_checked[] : count(1) = "a";

  if (p[0]) {
    a = 1;
    if (1[p]) {
      a = 2;
      if (p[2]) {
        a = 3;
        if (3[p]) {
          a = 4;
        }
        a = 5;
      }
      a = 6;
    }
    a = 7;
  }
  a = 8;

// CHECK: Function: f13
// CHECK: Block: B10 (Entry), Pred: Succ: B9

// CHECK: Block: B9, Pred: B10, Succ: B8, B1
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] : count(1) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B8, Pred: B9, Succ: B7, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: 1[p]
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B7, Pred: B8, Succ: B6, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK:   Widened bounds before stmt: p[2]
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B6, Pred: B7, Succ: B5, B4
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK:   Widened bounds before stmt: 3[p]
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 3 + 1)

// CHECK: Block: B4, Pred: B5, B6, Succ: B3
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B3, Pred: B4, B7, Succ: B2
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B2, Pred: B3, B8, Succ: B1
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B1, Pred: B2, B9, Succ: B0
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 1)
}

void f14(int i) {
  char p _Nt_checked[] : bounds(p, p + i) = "a";

  if ((1 + i)[p]) {
    a = 1;
    if (p[i]) {
      a = 2;
      if ((1 + i)[p]) {
        a = 3;
      }
      a = 4;
    }
    a = 5;
  }
  a = 6;

// CHECK: Function: f14
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] : bounds(p, p + i) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: (1 + i)[p]
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B6, Pred: B7, Succ: B5, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + i)

// CHECK:   Widened bounds before stmt: p[i]
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B5, Pred: B6, Succ: B4, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + i + 1)

// CHECK:   Widened bounds before stmt: (1 + i)[p]
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + (1 + i) + 1)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B2, Pred: B3, B6, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B1, Pred: B2, B7, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + i)
}

void f15(int i) {
  _Nt_array_ptr<char> p : bounds(p, p - i) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(p - i)) {
    a = 1;
  }

// CHECK: Function: f15
// CHECK: Block: B11 (Entry), Pred: Succ: B10

// CHECK: Block: B10, Pred: B11, Succ: B9, B8
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p - i) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p - i)
// CHECK:     p: bounds(p, p - i)

// CHECK: Block: B9, Pred: B10, Succ: B8
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p - i + 1)

  _Nt_array_ptr<char> q : count(0) = "a";
  if (*q) {
    a = 2;
    if (*(q - 1)) { // expected-error {{out-of-bounds memory access}}
      a = 3;
    }
  }

// CHECK: Block: B8, Pred: B9, B10, Succ: B7, B5
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : count(0) = "a";
// CHECK:     p: bounds(p, p - i)

// CHECK:   Widened bounds before stmt: *q
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B7, Pred: B8, Succ: B6, B5
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 1)

// CHECK:   Widened bounds before stmt: *(q - 1)
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 1)

  _Nt_array_ptr<char> r : bounds(r, r + +1) = "a";
  if (*(r + +1)) {
    a = 4;
  }

// CHECK: Block: B5, Pred: B6, B7, B8, Succ: B4, B3
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> r : bounds(r, r + +1) = "a";
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 0)

// CHECK:   Widened bounds before stmt: *(r + +1)
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 0)
// CHECK:     r: bounds(r, r + +1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 0)
// CHECK:     r: bounds(r, r + +1 + 1)

  _Nt_array_ptr<char> s : bounds(s, s + -1) = "a";
  if (*(s + -1)) { // expected-error {{out-of-bounds memory access}}
    a = 5;
  }

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> s : bounds(s, s + -1) = "a";
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 0)
// CHECK:     r: bounds(r, r + +1)

// CHECK:   Widened bounds before stmt: *(s + -1)
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 0)
// CHECK:     r: bounds(r, r + +1)
// CHECK:     s: bounds(s, s + -1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p - i)
// CHECK:     q: bounds(q, q + 0)
// CHECK:     r: bounds(r, r + +1)
// CHECK:     s: bounds(s, s + -1 + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f16(_Nt_array_ptr<char> p : bounds(p, p)) {
  _Nt_array_ptr<char> q : bounds(p, p) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'q' imply the declared bounds of 'q' after initialization}}
  _Nt_array_ptr<char> r : bounds(p, p + 1) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'r' imply the declared bounds of 'r' after initialization}}

  if (*(p)) {
    a = 1;
    if (*(p + 1)) {
      a = 2;
    }
    a = 3;
  }

// CHECK: Function: f16
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : bounds(p, p) = "a";
// CHECK:     p: bounds(p, p)

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> r : bounds(p, p + 1) = "a";
// CHECK:     p: bounds(p, p)
// CHECK:     q: bounds(p, p)

// CHECK:   Widened bounds before stmt: *(p)
// CHECK:     p: bounds(p, p)
// CHECK:     q: bounds(p, p)
// CHECK:     r: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, (p) + 1)
// CHECK:     q: bounds(p, (p) + 1)
// CHECK:     r: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, (p) + 1)
// CHECK:     q: bounds(p, (p) + 1)
// CHECK:     r: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(p, p + 1 + 1)
// CHECK:     r: bounds(p, p + 1 + 1)

// CHECK: Block: B2, Pred: B3, B4, Succ: B1
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, (p) + 1)
// CHECK:     q: bounds(p, (p) + 1)
// CHECK:     r: bounds(p, p + 1)

// CHECK: Block: B1, Pred: B2, B5, Succ: B0
}

void f17(char p _Nt_checked[] : count(1)) {
  _Nt_array_ptr<char> q : bounds(p, p + 1) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'q' imply the declared bounds of 'q' after initialization}}
  _Nt_array_ptr<char> r : bounds(p, p) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'r' imply the declared bounds of 'r' after initialization}}

  if (*(p)) {
    a = 1;
    if (*(p + 1)) {
      a = 2;
    }
  }

// CHECK: Function: f17
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : bounds(p, p + 1) = "a";
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> r : bounds(p, p) = "a";
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p)
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, p + 1)
// CHECK:     r: bounds(p, p)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, p + 1)
// CHECK:     r: bounds(p, (p) + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, p + 1)
// CHECK:     r: bounds(p, (p) + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(p, p + 1 + 1)
// CHECK:     r: bounds(p, p + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f18() {
  char p _Nt_checked[] = "a";
  char q _Nt_checked[] = "ab";
  char r _Nt_checked[] : count(0) = "ab";
  char s _Nt_checked[] : count(1) = "ab";

// CHECK: Function: f18
// CHECK: Block: B14 (Entry), Pred: Succ: B13

// CHECK: Block: B13, Pred: B14, Succ: B12, B10
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] = "a";

// CHECK:   Widened bounds before stmt: char q _Nt_checked[] = "ab";
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: char r _Nt_checked[] : count(0) = "ab";
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)

// CHECK:   Widened bounds before stmt: char s _Nt_checked[] : count(1) = "ab";
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)

  if (p[0]) {
    a = 1;
    if (p[1]) {
      a = 2;
    }
  }

// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B12, Pred: B13, Succ: B11, B10
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK:   Widened bounds before stmt: p[1]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B11, Pred: B12, Succ: B10
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

  if (q[0]) {
    a = 3;
    if (q[1]) {
      a = 4;
      if (q[2]) {
        a = 5;
      }
    }
  }

// CHECK: Block: B10, Pred: B11, B12, B13, Succ: B9, B6
// CHECK:   Widened bounds before stmt: q[0]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B9, Pred: B10, Succ: B8, B6
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK:   Widened bounds before stmt: q[1]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B8, Pred: B9, Succ: B7, B6
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK:   Widened bounds before stmt: q[2]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B7, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2 + 1)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

  if (r[0]) {
    a = 6;
  }

// CHECK: Block: B6, Pred: B7, B8, B9, B10, Succ: B5, B4
// CHECK:   Widened bounds before stmt: r[0]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0 + 1)
// CHECK:     s: bounds(s, s + 1)

  if (s[0]) {
    a = 7;
    if (s[1]) {
      a = 8;
    }
  }

// CHECK: Block: B4, Pred: B5, B6, Succ: B3, B1
// CHECK:   Widened bounds before stmt: s[0]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK:   Widened bounds before stmt: s[1]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f19() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p) {
    a = 1;
    if (*(p + 1)) {
      a = 2;
      if (*(p + 3)) {
        a = 3;
        if (*(p + 2)) {
          a = 4;
        }
      }
    }
  }

// CHECK: Function: f19
// CHECK: Block: B7 (Entry), Pred: Succ: B6

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK:   Widened bounds before stmt: *(p + 3)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, B6, Succ: B0
}

void f20_1() {
  // Pointer dereferenced at the upper bound. Valid bounds widening.
  _Nt_array_ptr<char> p : count(INT_MAX) = "";  // expected-error {{declared bounds for 'p' are invalid after initialization}}
  if (*(p + INT_MAX)) {
    a = 1;
  }

// CHECK: Function: f20_1
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count({{.*}}) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + {{.*}})
// CHECK:     p: bounds(p, p + {{.*}})

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + {{.*}} + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f20_2() {
  // Pointer dereferenced at the upper bound. Valid bounds widening.
  _Nt_array_ptr<char> q : count(INT_MIN) = "";
  if (*(q + INT_MIN)) {  // expected-error {{out-of-bounds memory access}}
    a = 2;
  }

// CHECK: Function: f20_2
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : count({{.*}}) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(q + {{.*}})
// CHECK:     q: bounds(q, q + {{.*}})

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     q: bounds(q, q + {{.*}} + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

// See issue #780 for a test involving INT_MAX that does not work on an Windows
// X86 build.
void f20_3() {
  _Nt_array_ptr<char> r : count(INT_MIN) = "";
  if (*(r + INT_MAX - 1)) { // expected-error {{out-of-bounds memory access}}
    a = 3;
  }

// CHECK: Function: f20_3
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> r : count({{.*}}) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(r + {{.*}} - 1)
// CHECK:     r: bounds(r, r + {{.*}})

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     r: bounds(r, r + {{.*}})

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f20_4() {
  // Pointer dereferenced at the upper bound but integer overflow occurs. No
  // bounds widening.
  _Nt_array_ptr<char> s : count(INT_MAX + 1) = "";
  if (*(s + INT_MAX + 1)) {  // expected-error {{out-of-bounds memory access}}
    a = 4;
  }

// CHECK: Function: f20_4
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> s : count({{.*}} + 1) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(s + {{.*}} + 1)
// CHECK:     s: bounds(s, s + {{.*}} + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     s: bounds(s, s + {{.*}} + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f20_5() {
  // Pointer dereferenced at the upper bound. Valid bounds widening.
  _Nt_array_ptr<char> t : count(INT_MIN + 1) = "";
  if (*(t + INT_MIN + 1)) {  // expected-error {{out-of-bounds memory access}}
    a = 5;
  }

// CHECK: Function: f20_5
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> t : count({{.*}} + 1) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(t + {{.*}} + 1)
// CHECK:     t: bounds(t, t + {{.*}} + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     t: bounds(t, t + {{.*}} + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f20_6() {
  // Pointer dereferenced at the upper bound but integer underflow occurs. No
  // bounds widening.
  _Nt_array_ptr<char> u : count(INT_MIN + -1) = ""; // expected-error {{declared bounds for 'u' are invalid after initialization}}
  if (*(u + INT_MIN + -1)) {
    a = 6;
  }

// CHECK: Function: f20_6
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> u : count({{.*}} + -1) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(u + {{.*}} + -1)
// CHECK:     u: bounds(u, u + {{.*}} + -1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     u: bounds(u, u + {{.*}} + -1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f21() {
  char p _Nt_checked[] : count(0) = "abc";

  while (p[0]) {
    a = 1;
    while (p[1]) {
      a = 2;
      while (p[2]) {
        a = 3;
      }
    }
  }

// CHECK: Function: f21
// CHECK: Block: B12 (Entry), Pred: Succ: B11

// CHECK: Block: B11, Pred: B12, Succ: B10
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] : count(0) = "abc";
// CHECK:     <no widening>

// CHECK: Block: B10, Pred: B2, B11, Succ: B9, B1
// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B9, Pred: B10, Succ: B8
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK: Block: B8, Pred: B3, B9, Succ: B7, B2
// CHECK:   Widened bounds before stmt: p[1]
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK: Block: B7, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B6, Pred: B4, B7, Succ: B5, B3
// CHECK:   Widened bounds before stmt: p[2]
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B6

// CHECK: Block: B3, Pred: B6, Succ: B8

// CHECK: Block: B2, Pred: B8, Succ: B10

// CHECK: Block: B1, Pred: B10, Succ: B0
}

void f22() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p) {
    a = 1;
    while (*(p + 1)) {
      a = 2;
      if (*(p + 2)) {
        a = 3;
      }
    }
  }

// CHECK: Function: f22
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B2, B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B2
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B2, Pred: B3, B4, Succ: B5

// CHECK: Block: B1, Pred: B5, B7, Succ: B0
}

void f23() {
  _Nt_array_ptr<char> p : count(0) = "";

  goto B;
  while (*p) {
B:  a = 1;
    while (*(p + 1)) { // expected-error {{out-of-bounds memory access}}
      a = 2;
    }
  }

// CHECK: Function: f23
// CHECK: Block: B16 (Entry), Pred: Succ: B15

// CHECK: Block: B15, Pred: B16, Succ: B13
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK: Block: B14, Pred: B9, Succ: B13, B8
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B13, Pred: B14, B15, Succ: B12
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B12, Pred: B10, B13, Succ: B11, B9
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B11, Pred: B12, Succ: B10
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B10, Pred: B11, Succ: B12

// CHECK: Block: B9, Pred: B12, Succ: B14

  while (*p) {
    a = 3;
    while (*(p + 1)) { // expected-error {{out-of-bounds memory access}}
C:    a = 4;
    }
  }
  goto C;

// CHECK: Block: B8, Pred: B3, B14, Succ: B7, B2
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B7, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B6, Pred: B4, B7, Succ: B5, B3
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B6, B2, Succ: B4
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B4, Pred: B5, Succ: B6

// CHECK: Block: B3, Pred: B6, Succ: B8

// CHECK: Block: B2, Pred: B8, Succ: B5
}

void f24() {
  _Nt_array_ptr<char> p : count(0) = "";

  while (*p) {
    p++;
    while (*(p + 1)) {
      a = 1;
    }
  }

// CHECK: Function: f24
// CHECK: Block: B9 (Entry), Pred: Succ: B8

// CHECK: Block: B8, Pred: B9, Succ: B7
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK: <no widening>

// CHECK: Block: B7, Pred: B2, B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   Widened bounds before stmt: p++
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B3, B6, Succ: B4, B2
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p - 1, p - 1 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p - 1, p - 1 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B5

// CHECK: Block: B2, Pred: B5, Succ: B7

// CHECK: Block: B1, Pred: B7, Succ: B0
}

void f25_1() {
  _Nt_array_ptr<char> p : count(0) = "";

  for (; *p; ) {
    a = 1;
    for (; *(p + 1); ) {
      a = 2;
      for (; *(p + 2); ) {
        a = 3;
      }
      a = 4;
    }
    a = 5;
  }
  a = 6;

// CHECK: Function: f25_1
// CHECK: Block: B21 (Entry), Pred: Succ: B20

// CHECK: Block: B20, Pred: B21, Succ: B19
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK: Block: B19, Pred: B9, B20, Succ: B18, B8
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B18, Pred: B19, Succ: B17
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B17, Pred: B11, B18, Succ: B16, B10
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B16, Pred: B17, Succ: B15
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B15, Pred: B13, B16, Succ: B14, B12
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B14, Pred: B15, Succ: B13
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B13, Pred: B14, Succ: B15

// CHECK: Block: B12, Pred: B15, Succ: B11
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B11, Pred: B12, Succ: B17

// CHECK: Block: B10, Pred: B17, Succ: B9
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B9, Pred: B10, Succ: B19

// CHECK: Block: B8, Pred: B19, Succ: B7
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 0)

  for (; *p; ) {
    p++;
    for (; *(p + 1); ) {
      a = 7;
    }
  }

// CHECK: Block: B7, Pred: B2, B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   Widened bounds before stmt: p++
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B3, B6, Succ: B4, B2
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p - 1, p - 1 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p - 1, p - 1 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B5

// CHECK: Block: B2, Pred: B5, Succ: B7

// CHECK: Block: B1, Pred: B7, Succ: B0
}

void f25_2() {
  _Nt_array_ptr<char> p : count(0) = "";

  for (; *p; ) {
D:  a = 1;
    for (; *(p + 1); ) { // expected-error {{out-of-bounds memory access}}
      a = 2;
    }
  }
  goto D;

// CHECK: Function: f25_2
// CHECK: Block: B10 (Entry), Pred: Succ: B9

// CHECK: Block: B9, Pred: B10, Succ: B8
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK: Block: B8, Pred: B3, B9, Succ: B7, B2
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B7, Pred: B8, B2, Succ: B6
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B4, B7, Succ: B5, B3
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)
}

void f26() {
  _Nt_array_ptr<char> p : bounds(p, ((((((p + 1))))))) = "a";

  if (*(((p + 1)))) {
    a = 1;
  }

// CHECK: Function: f26
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, ((((((p + 1))))))) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(((p + 1)))
// CHECK:     p: bounds(p, ((((((p + 1)))))))

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f27(_Nt_array_ptr<char> p : count(i), int i) {
  if (*(p + i)) {
    a == 1 ? i++ : i;  // expected-error {{inferred bounds for 'p' are unknown after increment}}

    if (*(p + i + 1)) {
      a = 2;
    }
  }

// CHECK: Function: f27
// CHECK: Block: B7 (Entry), Pred: Succ: B6

// CHECK: Block: B6, Pred: B7, Succ: B5, B0
// CHECK:   Widened bounds before stmt: *(p + i)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B5, Pred: B6, Succ: B3, B4
// CHECK:   Widened bounds before stmt: a == 1
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B4, Pred: B5, Succ: B2
// CHECK:   Widened bounds before stmt: i
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B3, Pred: B5, Succ: B2
// CHECK:   Widened bounds before stmt: i++
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B2, Pred: B3, B4, Succ: B1, B0
// CHECK:   Widened bounds before stmt: a == 1 ? i++ : i
// CHECK:     p: bounds(p, p + i + 1)

// CHECK:   Widened bounds before stmt: *(p + i + 1)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B1, Pred: B2, Succ: B0
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + i)
}

void f28() {
  _Nt_array_ptr<char> p : count(0) = "";

  switch (*p) {
  default: a = 0; break;
  case 'a': a = 1; break;
  case 'b': a = 2; break;
  }

// CHECK: Function: f28
// CHECK: Block: B22 (Entry), Pred: Succ: B18

// CHECK: Block: B21, Pred: B18, Succ: B14
// CHECK:   Widened bounds before stmt: a = 0
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B20, Pred: B18, Succ: B14
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B19, Pred: B18, Succ: B14
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1)

  switch (*p) {
  case 'a': a = 3;
  default: a = 4;
  case 'b': a = 5; break;
  }

// CHECK: Block: B18, Pred: B22, Succ: B19, B20, B21
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B17, Pred: B14, Succ: B16
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B16, Pred: B17, B14, Succ: B15
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B15, Pred: B14, B16, Succ: B10
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 0)

  switch (*p) {
  case 'a': a = 6; break;
  default: a = 7;
  case 'b': a = 8; break;
  }

// CHECK: Block: B14, Pred: B19, B20, B21, Succ: B15, B17, B16
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B13, Pred: B10, Succ: B6
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B12, Pred: B10, Succ: B11
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B11, Pred: B10, B12, Succ: B6
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 0)

  switch (*p) {
  default: a = 9; break;
  case '\0': a = 10; break;
  case 'a': a = 11; break;
  }

// CHECK: Block: B10, Pred: B15, Succ: B11, B13, B12
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B9, Pred: B6, Succ: B2
// CHECK:   Widened bounds before stmt: a = 9
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B8, Pred: B6, Succ: B2
// CHECK:   Widened bounds before stmt: a = 10
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B7, Pred: B6, Succ: B2
// CHECK:   Widened bounds before stmt: a = 11
// CHECK:     p: bounds(p, p + 1)

  switch (*p) {
  case '\0': a = 12;
  case 'a': a = 13; break;
  default: a = 14; break;
  }

// CHECK: Block: B6, Pred: B11, B13, Succ: B7, B8, B9
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B2, Succ: B4
// CHECK:   Widened bounds before stmt: a = 12
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B4, Pred: B2, B5, Succ: B1
// CHECK:   Widened bounds before stmt: a = 13
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: a = 14
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B7, B8, B9, Succ: B4, B5, B3
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, B4, Succ: B0
}

void f29() {
  _Nt_array_ptr<char> p : count(0) = "";
  const char c1 = '\0';
  const int c2 = '0';
  const char c3 = '1';
  const int c4 = '2';

  switch (*p) {
  default: a = 1; break;

  case c1: a = 2; break;

  case c2: a = 3; break;

  case c3: a = 4; break;

  case c4: a = 5; break;
  }

// CHECK: Function: f29
// CHECK: Block: B12 (Entry), Pred: Succ: B6
// CHECK: Block: B11, Pred: B6, Succ: B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B10, Pred: B6, Succ: B2
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B9, Pred: B6, Succ: B2
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B8, Pred: B6, Succ: B2
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B7, Pred: B6, Succ: B2
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 1)

  enum {e1, e2};
  switch (*p) {
  case e1: a = 6; break;

  case e2: a = 7; break;

  default: a = 8; break;
  }

// CHECK: Block: B5, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B4, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B7, B8, B9, B10, B11, Succ: B4, B5, B3
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, B4, B5, Succ: B0
}

void f30() {
  _Nt_array_ptr<char> p : count(0) = "";

  switch (*p) {
  default: a = 1; break;

  case 0: a = 2;
    switch (*p) {
      default: a = 3; break;
      case 1: a = 4; break;
    }
  }

// CHECK: Function: f30
// CHECK: Block: B15 (Entry), Pred: Succ: B10

// CHECK: Block: B14, Pred: B10, Succ: B7
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B13, Pred: B11, Succ: B7
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B12, Pred: B11, Succ: B7
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B11, Pred: B10, Succ: B12, B13
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B10, Pred: B15, Succ: B11, B14
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

  switch (*p) {
  case 1: a = 5;
    switch (*(p + 1)) {
      case 2: a = 6; break;
    }
  }

// CHECK: Block: B9, Pred: B8, Succ: B2
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B8, Pred: B7, Succ: B9, B2
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B7, Pred: B12, B13, B14, Succ: B8, B2
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

  switch (*p) {
  case 1: a = 7; do { a = 8;
                 } while (a > 0);
  }

// CHECK: Block: B6, Pred: B2, Succ: B4
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B3, Succ: B4

// CHECK: Block: B4, Pred: B5, B6, Succ: B3
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B4, Succ: B5, B1
// CHECK:   Widened bounds before stmt: a > 0
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B9, B8, B7, Succ: B6, B1
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, B2, Succ: B0
}

void f31() {
  _Nt_array_ptr<char> p : count(0) = "";

  switch (*p) {
  case 'a':
    a = 1;

    if (*(p + 1)) {
      a = 2;

      for (;*(p + 2);) {
        a = 3;

        while (*(p + 3)) {
          a = 4;
        }
        a = 5;
      }
      a = 6;
    }
    a = 7;

    break;
  }
  a = 8;

// CHECK: Function: f31
// CHECK: Block: B14 (Entry), Pred: Succ: B2

// CHECK: Block: B13, Pred: B2, Succ: B12, B3
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B12, Pred: B13, Succ: B11
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B11, Pred: B5, B12, Succ: B10, B4
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B10, Pred: B11, Succ: B9
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B9, Pred: B7, B10, Succ: B8, B6
// CHECK:   Widened bounds before stmt: *(p + 3)
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B8, Pred: B9, Succ: B7
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 3 + 1)

// CHECK: Block: B7, Pred: B8, Succ: B9

// CHECK: Block: B6, Pred: B9, Succ: B5
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B5, Pred: B6, Succ: B11

// CHECK: Block: B4, Pred: B11, Succ: B3
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B3, Pred: B4, B13, Succ: B1
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B14, Succ: B13, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, B2, Succ: B0
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 0)
}

void f32() {
  _Nt_array_ptr<char> p : count(0) = "";
  const int i = -1;
  const int j = 1;

  switch (*p) {
  case i ... j: a = 1; break;
  }

// CHECK: Function: f32
// CHECK: Block: B14 (Entry), Pred: Succ: B12

// CHECK: Block: B13, Pred: B12, Succ: B8
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)

  switch (*p) {
  case 1 ... -1: a = 2; break;

  case 1 ... 0: a = 3; break;

  case -2 ... -1: a = 4; break;
  }

// CHECK: Block: B11, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B10, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B9, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)

  switch (*p) {
  case -1 ... 1: a = 5; break;
  }

// CHECK: Block: B7, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 0)

  switch (*p) {
  case 0 ... 1: a = 6; break;
  }

// CHECK: Block: B5, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 0)

  switch (*p) {
  case 1 ... 2: a = 7; break;
  }

// CHECK: Block: B3, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)
}

void f34() {
  _Nt_array_ptr<char> p : count(0) = "";
  const int i = 'abc';
  const char c = 'xyz';
  const unsigned u = UINT_MAX;

  switch (*p) {
  case 999999999999999999999999999: a = 1; break; // expected-error {{integer literal is too large to be represented in any integer type}}

  case 00000000000000000000000000000000000: a = 2; break;
  case 00000000000000000000000000000000001: a = 3; break;
  case '00000000000000000000000000000000000': a = 4; break;

  case i: a = 5; break;
  case c: a = 6; break;
  case u: a = 7; break;
  }

// CHECK: Function: f34
// CHECK: Block: B25 (Entry), Pred: Succ: B17
// CHECK: Block: B24, Pred: B17, Succ: B11
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B23, Pred: B17, Succ: B11
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B22, Pred: B17, Succ: B11
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B21, Pred: B17, Succ: B11
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B20, Pred: B17, Succ: B11
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B19, Pred: B17, Succ: B11
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B18, Pred: B17, Succ: B11
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)

  switch (*p) {
  case INT_MAX: a = 8; break;
  case INT_MIN: a = 9; break;
  case INT_MAX + INT_MAX: a = 10; break;
  case INT_MAX - INT_MAX: a = 11; break;
  case INT_MAX + INT_MIN: a = 12; break;
  }

// CHECK: Block: B16, Pred: B11, Succ: B8
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B15, Pred: B11, Succ: B8
// CHECK:   Widened bounds before stmt: a = 9
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B14, Pred: B11, Succ: B8
// CHECK:   Widened bounds before stmt: a = 10
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B13, Pred: B11, Succ: B8
// CHECK:   Widened bounds before stmt: a = 11
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B12, Pred: B11, Succ: B8
// CHECK:   Widened bounds before stmt: a = 12
// CHECK:     p: bounds(p, p + 1)

  switch (*p) {
  case INT_MIN - INT_MIN: a = 13; break;
  case INT_MAX - INT_MIN: a = 14; break;
  }

// CHECK: Block: B10, Pred: B8, Succ: B5
// CHECK:   Widened bounds before stmt: a = 13
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B9, Pred: B8, Succ: B5
// CHECK:   Widened bounds before stmt: a = 14
// CHECK:     p: bounds(p, p + 1)

  switch (*p) {
  // Note: This does not widen the bounds as the value of the expression is
  // computed to 0 and we have the warning: overflow in expression; result is 0
  // with type 'int'.
  case INT_MIN + INT_MIN: a = 15; break;

  case UINT_MAX: a = 16; break;
  }

// CHECK: Block: B7, Pred: B5, Succ: B2
// CHECK:   Widened bounds before stmt: a = 15
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B5, Succ: B2
// CHECK:   Widened bounds before stmt: a = 16
// CHECK:     p: bounds(p, p + 1)

  _Nt_array_ptr<uint64_t> q : count(0) = 0;
  const uint64_t x = 0x0000444400004444LL;

  switch (*p) {
    case ULLONG_MAX: a = 17; break;
    case x: a = 18; break;
  }

// CHECK: Block: B4, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: a = 17
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B3, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: a = 18
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 0)
}

void f34_1() {
  _Nt_array_ptr<char> p : count(0) = "";

  switch(*p) {
    default: f1(); break;
    case 0: {
      a = 1;

      switch(*p) {
        default: f2(); break;
        case 'a': a = 2; break;
      }
      break;
    }
  }

// CHECK: Function: f34_1
// CHECK: Block: B8 (Entry), Pred: Succ: B2

// CHECK: Block: B7, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: f1()
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B6, Pred: B4, Succ: B3
// CHECK:   Widened bounds before stmt: f2()
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B4, Succ: B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B2, Succ: B5, B6
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B5, B6, Succ: B1

// CHECK: Block: B2, Pred: B8, Succ: B4, B7
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, B7, Succ: B0
}

void f34_2() {
  _Nt_array_ptr<char> p : count(0) = "";

  switch(*p) {
    default: f1(); break;
    case 0: {
      a = 3;

      switch(*p) {
        default: f2(); break;
        case '\0': a = 4; break;
      }
      break;
    }
  }

// CHECK: Function: f34_2
// CHECK: Block: B8 (Entry), Pred: Succ: B2

// CHECK: Block: B7, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: f1()
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B6, Pred: B4, Succ: B3
// CHECK:   Widened bounds before stmt: f2()
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B4, Succ: B3
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B4, Pred: B2, Succ: B5, B6
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B5, B6, Succ: B1

// CHECK: Block: B2, Pred: B8, Succ: B4, B7
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, B7, Succ: B0
}

void f34_3() {
  _Nt_array_ptr<char> p : count(0) = "";

  switch(*p) {
    default: f1(); break;
    case 'b': {
      a = 5;

      switch(*(p + 1)) {
        default: f2(); break;
        case 'a': a = 6; break;
      }
      break;
    }
  }

// CHECK: Function: f34_3
// CHECK: Block: B8 (Entry), Pred: Succ: B2

// CHECK: Block: B7, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: f1()
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B4, Succ: B3
// CHECK:   Widened bounds before stmt: f2()
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B4, Succ: B3
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B4, Pred: B2, Succ: B5, B6
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B5, B6, Succ: B1

// CHECK: Block: B2, Pred: B8, Succ: B4, B7
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, B7, Succ: B0
}

void f34_4() {
  _Nt_array_ptr<char> p : count(0) = "";

  switch(*p) {
    default: f1(); break;
    case 'b': {
      a = 7;

      switch(*(p + 1)) {
        default: f2(); break;
        case '\0': a = 8; break;
      }
      break;
    }
  }

// CHECK: Function: f34_4
// CHECK: Block: B8 (Entry), Pred: Succ: B2

// CHECK: Block: B7, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: f1()
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B4, Succ: B3
// CHECK:   Widened bounds before stmt: f2()
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B5, Pred: B4, Succ: B3
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B2, Succ: B5, B6
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B5, B6, Succ: B1

// CHECK: Block: B2, Pred: B8, Succ: B4, B7
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, B7, Succ: B0
}

void f34_5() {
  _Nt_array_ptr<char> p : count(0) = "";

  switch(*p) {
    default: {
      a = 9;

      switch(*(p + 1)) {
        default: f2(); break;
        case '\0': a = 10; break;
      }
      break;
    }
    case 0: a = 11; break;
  }

// CHECK: Function: f34_5
// CHECK: Block: B8 (Entry), Pred: Succ: B2

// CHECK: Block: B7, Pred: B5, Succ: B4
// CHECK:   Widened bounds before stmt: f2()
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B6, Pred: B5, Succ: B4
// CHECK:   Widened bounds before stmt: a = 10
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B2, Succ: B6, B7
// CHECK:   Widened bounds before stmt: a = 9
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B6, B7, Succ: B1

// CHECK: Block: B3, Pred: B2, Succ: B1
// CHECK:   Widened bounds before stmt: a = 11
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B2, Pred: B8, Succ: B3, B5
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, B4, Succ: B0
}

void f35() {
  _Nt_array_ptr<char> p : count(0) = "";

  if (*p) {
    a = 1;
A:  a = 2;
    a = 3;
  }

// CHECK: Function: f35
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B1, Pred: B2, B4, Succ: B0
}

void f36() {
  _Nt_array_ptr<char> p : count(0) = "";

  if (a > 0) {
    a = 1;
A:  a = 2;
    a = 3;

  } else if (a == 0) {
    a = 4;

    while (a != 0) {
      ++a;

      switch(a) {
      default: a = 5; break;
      case 1: a = 6; break;
      }
    }
  }

  if (*p) {
    a = 7;
  }

  goto A;

// CHECK: Function: f36
// CHECK: Block: B15 (Entry), Pred: Succ: B14

// CHECK: Block: B14, Pred: B15, Succ: B13, B11
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: a > 0
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B13, Pred: B14, Succ: B12
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B12, Pred: B13, B2, Succ: B4
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B11, Pred: B14, Succ: B10, B4
// CHECK:   Widened bounds before stmt: a == 0
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B10, Pred: B11, Succ: B9
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B9, Pred: B5, B10, Succ: B6, B4
// CHECK:   Widened bounds before stmt: a != 0
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B8, Pred: B6, Succ: B5
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B7, Pred: B6, Succ: B5
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B9, Succ: B7, B8
// CHECK:   Widened bounds before stmt: ++a
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: a
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B7, B8, Succ: B9

// CHECK: Block: B4, Pred: B9, B11, B12, Succ: B3, B2
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, B4, Succ: B12
}
