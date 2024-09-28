// Tests for datafow analysis for bounds widening in case of conditionals with
// multiple dereferences of null-terminated arrays.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify \
// RUN: -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s \
// RUN: | FileCheck %s

int a;

void f1() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p && *(p + 1) && *(p + 2)) {
    a = 1;
  }

// CHECK: Function: f1
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f2() {
  _Nt_array_ptr<char> p : count(0) = "a";
  int a;

  if (*p && *(p + 1)) {
    p = "b";
    a = 1;
  }

// CHECK: Function: f2
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: int a;
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: p = "b"
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f3(_Nt_array_ptr<char> p : count(0)) {
  if ((((0[p]))) && (((p[1]))) && ((p[2]))) {
    a = 1;
  }

// CHECK: Function: f3
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B0
// CHECK:   Widened bounds before stmt: (((0[p])))
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B4, Succ: B2, B0
// CHECK:   Widened bounds before stmt: (((p[1])))
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1, B0
// CHECK:   Widened bounds before stmt: ((p[2]))
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B1, Pred: B2, Succ: B0
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 2 + 1)
}

void f4() {
  char p _Nt_checked[] : count(0) = "abc";

  if (p[0] && p[1] && p[2]) {
    a = 1;
  }

// CHECK: Function: f4
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] : count(0) = "abc";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: p[1]
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: p[2]
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f5(int i) {
  char p _Nt_checked[] : bounds(p + i, p)  = "abc";

  if (p[0] && p[1]) {
    i = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    if (p[2]) {
      a = 1;
    }
    a = 2;
  }
  a = 3;

// CHECK: Function: f5
// CHECK: Block: B7 (Entry), Pred: Succ: B6

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] : bounds(p + i, p) = "abc";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p + i, p)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: p[1]
// CHECK:     p: bounds(p + i, p + 0 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B2
// CHECK:   Widened bounds before stmt: i = 0
// CHECK:     p: bounds(p + i, p + 1 + 1)

// CHECK:   Widened bounds before stmt: p[2]
// CHECK:     p: bounds(p + i, p)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p + i, p)

// CHECK: Block: B2, Pred: B3, B4, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p + i, p)

// CHECK: Block: B1, Pred: B2, B5, B6, Succ: B0
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p + i, p)
}

void f6() {
  _Nt_array_ptr<char> p : count(2) = "abc";

  if (*p && *(p + 1) && *(p + 2) && *(p + 3)) {
    a = 1;
  }

// CHECK: Function: f6
// CHECK: Block: B7 (Entry), Pred: Succ: B6

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(2) = "abc";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 2)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 2)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 2)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 3)
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 3 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, B6, Succ: B0
}

void f7() {
  _Nt_array_ptr<char> p : count(0) = "abc";

  if (*p && *(p + 2) && *(p + 3) && *(p + 1)) { // expected-error {{out-of-bounds memory access}} expected-error {{out-of-bounds memory access}}
    a = 1;
  }

// CHECK: Function: f7
// CHECK: Block: B7 (Entry), Pred: Succ: B6

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "abc";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + 3)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, B6, Succ: B0
}

void f8() {
  _Nt_array_ptr<char> p : count(0) = "abc";

  if (*p && *(p + 2) && *(p + 1) && *(p + 3) && *(p + 2) && *(p + 3)) { // expected-error {{out-of-bounds memory access}}
    a = 1;
  }

// CHECK: Function: f8
// CHECK: Block: B9 (Entry), Pred: Succ: B8

// CHECK: Block: B8, Pred: B9, Succ: B7, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "abc";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *(p + 3)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 3)
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 3 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, B6, B7, B8, Succ: B0
}

void f9(int i) {
  _Nt_array_ptr<char> p : bounds(p, p + i) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*p && *(p + i) && *(p + i + 1)) {
    a = 1;
  }

// CHECK: Function: f9
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + i)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + i + 1)
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + i + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f10(int i) {
  _Nt_array_ptr<char> p : bounds(p, 1 + p + i + 5) = "a";

  if (*(i + p + 1 + 2 + 3) && *(3 + p + i + 4) && *(p + i + 9)) {
    a = 1;
  }

// CHECK: Function: f10
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, 1 + p + i + 5) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(i + p + 1 + 2 + 3)
// CHECK:     p: bounds(p, 1 + p + i + 5)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(3 + p + i + 4)
// CHECK:     p: bounds(p, i + p + 1 + 2 + 3 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + i + 9)
// CHECK:     p: bounds(p, 3 + p + i + 4 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, 3 + p + i + 4 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f11_1(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p + i, p + j) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(p + j) && *(p + j + 1)) {
    i = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    if (*(p + j + 2)) {
      a = 1;
    }
  }

// CHECK: Function: f11_1
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p + i, p + j) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + j)
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + j + 1)
// CHECK:     p: bounds(p + i, p + j + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: i = 0
// CHECK:     p: bounds(p + i, p + j + 1 + 1)

// CHECK:   Widened bounds before stmt: *(p + j + 2)
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f11_2(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p + i, p + j) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(p + j) && *(p + j + 1)) {
    j = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    if (*(p + j + 2)) {
      a = 1;
    }
  }

// CHECK: Function: f11_2
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p + i, p + j) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + j)
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + j + 1)
// CHECK:     p: bounds(p + i, p + j + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: j = 0
// CHECK:     p: bounds(p + i, p + j + 1 + 1)

// CHECK:   Widened bounds before stmt: *(p + j + 2)
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p + i, p + j)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f12(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";

  if (*(((p + i + j))) && *((p) + (1) + (i) + (0) + (j)) && *(1 + (p + i + j) + 1)) {
    a = 1;
  }

// CHECK: Function: f12
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(((p + i + j)))
// CHECK:     p: bounds(p, p + i + j)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *((p) + (1) + (i) + (0) + (j))
// CHECK:     p: bounds(p, p + i + j + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(1 + (p + i + j) + 1)
// CHECK:     p: bounds(p, (p) + (1) + (i) + (0) + (j) + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, 1 + (p + i + j) + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f13() {
  char p _Nt_checked[] : count(1) = "a";

  if (p[0] && 1[p] && p[2] && 3[p]) {
    a = 1;
  }

// CHECK: Function: f13
// CHECK: Block: B7 (Entry), Pred: Succ: B6

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] : count(1) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: 1[p]
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: p[2]
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: 3[p]
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 3 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, B6, Succ: B0
}

void f14(int i) {
  char p _Nt_checked[] : bounds(p, p + i) = "a";

  if ((1 + i)[p] && p[i] && (2 + i + 1 - 1 + -1)[p]) {
    a = 1;
  }

// CHECK: Function: f14
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] : bounds(p, p + i) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: (1 + i)[p]
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: p[i]
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: (2 + i + 1 - 1 + -1)[p]
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + (2 + i + 1 - 1 + -1) + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f15_1(int i) {
  _Nt_array_ptr<char> p : bounds(p, p - i) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}
  if (*(p - i) && *(p - i + 1)) {
    a = 1;
  }

// CHECK: Function: f15_1
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p - i) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p - i)
// CHECK:     p: bounds(p, p - i)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p - i + 1)
// CHECK:     p: bounds(p, p - i + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p - i + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f15_2(int i) {
  _Nt_array_ptr<char> q : count(0) = "a";
  if (*q && *(q - 1)) { // expected-error {{out-of-bounds memory access}}
    a = 1;
  }

// CHECK: Function: f15_2
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *q
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(q - 1)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f15_3() {
  _Nt_array_ptr<char> r : bounds(r, r + +1) = "a";
  if (*(r + +1) && *(r + +2)) {
    a = 1;
  }

// CHECK: Function: f15_3
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> r : bounds(r, r + +1) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(r + +1)
// CHECK:     r: bounds(r, r + +1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(r + +2)
// CHECK:     r: bounds(r, r + +1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     r: bounds(r, r + +2 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f15_4() {
  _Nt_array_ptr<char> s : bounds(s, s + -1) = "a";
  if (*(s + -1) && *(s) && *(s + +1)) { // expected-error {{out-of-bounds memory access}}
    a = 1;
  }

// CHECK: Function: f15_4
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> s : bounds(s, s + -1) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(s + -1)
// CHECK:     s: bounds(s, s + -1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(s)
// CHECK:     s: bounds(s, s + -1 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(s + +1)
// CHECK:     s: bounds(s, (s) + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     s: bounds(s, s + +1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f16(_Nt_array_ptr<char> p : bounds(p, p)) {
  _Nt_array_ptr<char> q : bounds(p, p) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'q' imply the declared bounds of 'q' after initialization}}
  _Nt_array_ptr<char> r : bounds(p, p + 1) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'r' imply the declared bounds of 'r' after initialization}}

  if (*(p) && *(p + 1)) {
    a = 1;
  }

  p = "b"; // expected-error {{inferred bounds for 'q' are unknown after assignment}} expected-error {{inferred bounds for 'r' are unknown after assignment}}
  a = 2;

// CHECK: Function: f16
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : bounds(p, p) = "a";
// CHECK:     p: bounds(p, p)

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> r : bounds(p, p + 1) = "a";
// CHECK:     p: bounds(p, p)
// CHECK:     q: bounds(p, p)

// CHECK:   Widened bounds before stmt: *(p)
// CHECK:     p: bounds(p, p)
// CHECK:     q: bounds(p, p)
// CHECK:     r: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, (p) + 1)
// CHECK:     q: bounds(p, (p) + 1)
// CHECK:     r: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(p, p + 1 + 1)
// CHECK:     r: bounds(p, p + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
// CHECK:   Widened bounds before stmt: p = "b"
// CHECK:     p: bounds(p, p)
// CHECK:     q: bounds(p, p)
// CHECK:     r: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p)
// CHECK:     q: bounds(p, p)
// CHECK:     r: bounds(p, p + 1)
}

void f17(char p _Nt_checked[] : count(1)) {
  _Nt_array_ptr<char> q : bounds(p, p + 1) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'q' imply the declared bounds of 'q' after initialization}}
  _Nt_array_ptr<char> r : bounds(p, p) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'r' imply the declared bounds of 'r' after initialization}}

  if (*(p) && *(p + 1)) {
    a = 1;
  }

  p = "b"; // expected-error {{inferred bounds for 'q' are unknown after assignment}} expected-error {{inferred bounds for 'r' are unknown after assignment}}
  a = 2;

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
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, p + 1)
// CHECK:     r: bounds(p, (p) + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(p, p + 1 + 1)
// CHECK:     r: bounds(p, p + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
// CHECK:   Widened bounds before stmt: p = "b"
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, p + 1)
// CHECK:     r: bounds(p, p)

// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, p + 1)
// CHECK:     r: bounds(p, p)
}

void f18() {
  char p _Nt_checked[] = "a";
  char q _Nt_checked[] = "ab";
  char r _Nt_checked[] : count(0) = "ab";
  char s _Nt_checked[] : count(1) = "ab";

// CHECK: Function: f18
// CHECK: Block: B15 (Entry), Pred: Succ: B14

// CHECK: Block: B14, Pred: B15, Succ: B13, B11
// CHECK:   Widened bounds before stmt: char p _Nt_checked[] = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: char q _Nt_checked[] = "ab";
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: char r _Nt_checked[] : count(0) = "ab";
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)

// CHECK:   Widened bounds before stmt: char s _Nt_checked[] : count(1) = "ab";
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)

  if (p[0] && p[1]) {
    a = 1;
  }

// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B13, Pred: B14, Succ: B12, B11
// CHECK:   Widened bounds before stmt: p[1]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B12, Pred: B13, Succ: B11
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

  if (q[0] && q[1] && q[2]) {
    a = 2;
  }

// CHECK: Block: B11, Pred: B12, B13, B14, Succ: B10, B7
// CHECK:   Widened bounds before stmt: q[0]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B10, Pred: B11, Succ: B9, B7
// CHECK:   Widened bounds before stmt: q[1]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B9, Pred: B10, Succ: B8, B7
// CHECK:   Widened bounds before stmt: q[2]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B8, Pred: B9, Succ: B7
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2 + 1)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

  if (r[0] && r[1]) {
    a = 3;
  }

// CHECK: Block: B7, Pred: B8, B9, B10, B11, Succ: B6, B4
// CHECK:   Widened bounds before stmt: r[0]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B6, Pred: B7, Succ: B5, B4
// CHECK:   Widened bounds before stmt: r[1]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0 + 1)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 1 + 1)
// CHECK:     s: bounds(s, s + 1)

  if (s[0] && s[1]) {
    a = 4;
  }

// CHECK: Block: B4, Pred: B5, B6, B7, Succ: B3, B1
// CHECK:   Widened bounds before stmt: s[0]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: s[1]
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 2)
// CHECK:     r: bounds(r, r + 0)
// CHECK:     s: bounds(s, s + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f19() {
  _Nt_array_ptr<char> p : bounds(p, ((((((p + 1))))))) = "a";

  if (*(((p + 1))) && *(((p + 2)))) {
    a = 1;
  }

// CHECK: Function: f19
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, ((((((p + 1))))))) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(((p + 1)))
// CHECK:     p: bounds(p, ((((((p + 1)))))))

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(((p + 2)))
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f20(_Nt_array_ptr<char> p : count(i), int i) {
  if (*(p + i) && *(p + i + 1)) {
    a == 1 ? i++ : a++;  // expected-error {{inferred bounds for 'p' are unknown after increment}}

    if (*(p + i + 2)) {
      a = 3;
    }
  }

// CHECK: Function: f20
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B0
// CHECK:   Widened bounds before stmt: *(p + i)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B6, Pred: B7, Succ: B5, B0
// CHECK:   Widened bounds before stmt: *(p + i + 1)
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B5, Pred: B6, Succ: B3, B4
// CHECK:   Widened bounds before stmt: a == 1
// CHECK:     p: bounds(p, p + i + 1 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B2
// CHECK:   Widened bounds before stmt: a++
// CHECK:     p: bounds(p, p + i + 1 + 1)

// CHECK: Block: B3, Pred: B5, Succ: B2
// CHECK:   Widened bounds before stmt: i++
// CHECK:     p: bounds(p, p + i + 1 + 1)

// CHECK: Block: B2, Pred: B3, B4, Succ: B1, B0
// CHECK:   Widened bounds before stmt: a == 1 ? i++ : a++
// CHECK:     p: bounds(p, p + i + 1 + 1)

// CHECK:   Widened bounds before stmt: *(p + i + 2)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B1, Pred: B2, Succ: B0
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + i)
}

void f21() {
  _Nt_array_ptr<char> p : count(0) = "";

  if (((*p && *(p + 1)) && *(p + 2)) && (*(p + 3) && *(p + 4))) {
    a = 1;
  }

// CHECK: Function: f21
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + 3)
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 4)
// CHECK:     p: bounds(p, p + 3 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 4 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, B6, B7, Succ: B0
}

void f22() {
  _Nt_array_ptr<char> p : count(0) = "";
  _Nt_array_ptr<char> q : count(0) = "";

  if (((*p && *(p + 1))) && *(p + 2) && (*q && *(q + 1)) && (*(p + 3) && *(p + 4))) {
    a = 1;
  }

// CHECK: Function: f22
// CHECK: Block: B10 (Entry), Pred: Succ: B9

// CHECK: Block: B9, Pred: B10, Succ: B8, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : count(0) = "";
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B8, Pred: B9, Succ: B7, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: *q
// CHECK:     p: bounds(p, p + 2 + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *(q + 1)
// CHECK:     p: bounds(p, p + 2 + 1)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + 3)
// CHECK:     p: bounds(p, p + 2 + 1)
// CHECK:     q: bounds(q, q + 1 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 4)
// CHECK:     p: bounds(p, p + 3 + 1)
// CHECK:     q: bounds(q, q + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 4 + 1)
// CHECK:     q: bounds(q, q + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, B6, B7, B8, B9, Succ: B0
}

void f23() {
  _Nt_array_ptr<char> p : count(0) = "";
  _Nt_array_ptr<char> q : count(1) = "a";
  _Nt_array_ptr<char> r : count(2) = "ab";

  if (*(r + 2) && *(p) && *(1 + q) && *(1 + p) && *(r + 3) && *(q + 2)) {
    a = 1;
  }

// CHECK: Function: f23
// CHECK: Block: B9 (Entry), Pred: Succ: B8

// CHECK: Block: B8, Pred: B9, Succ: B7, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : count(1) = "a";
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> r : count(2) = "ab";
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)

// CHECK:   Widened bounds before stmt: *(r + 2)
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)
// CHECK:     r: bounds(r, r + 2)

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: *(p)
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)
// CHECK:     r: bounds(r, r + 2 + 1)

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: *(1 + q)
// CHECK:     p: bounds(p, (p) + 1)
// CHECK:     q: bounds(q, q + 1)
// CHECK:     r: bounds(r, r + 2 + 1)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *(1 + p)
// CHECK:     p: bounds(p, (p) + 1)
// CHECK:     q: bounds(q, 1 + q + 1)
// CHECK:     r: bounds(r, r + 2 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(r + 3)
// CHECK:     p: bounds(p, 1 + p + 1)
// CHECK:     q: bounds(q, 1 + q + 1)
// CHECK:     r: bounds(r, r + 2 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(q + 2)
// CHECK:     p: bounds(p, 1 + p + 1)
// CHECK:     q: bounds(q, 1 + q + 1)
// CHECK:     r: bounds(r, r + 3 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, 1 + p + 1)
// CHECK:     q: bounds(q, q + 2 + 1)
// CHECK:     r: bounds(r, r + 3 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, B6, B7, B8, Succ: B0
}

void f24() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p || *(p + 1)) { // expected-error {{out-of-bounds memory access}}
    a = 1;
  }

// CHECK: Function: f24
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B2, B3
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B2, Pred: B3, B4, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f25() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if ((*p || *(p + 1)) && *p && (*(p + 1) || *p) && *(p + 1)) { // expected-error {{out-of-bounds memory access}}
    a = 1;
  }

// CHECK: Function: f25
// CHECK: Block: B9 (Entry), Pred: Succ: B8

// CHECK: Block: B8, Pred: B9, Succ: B6, B7
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B7, Pred: B8, Succ: B6, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B6, Pred: B7, B8, Succ: B5, B1
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B6, Succ: B3, B4
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B6, B7, Succ: B0
}

void f26() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p && a > 0) {
    a = 1;
  }

// CHECK: Function: f26
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a > 0
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f27() {
  _Nt_array_ptr<char> p : count(0) = "a";

  while (*p && *(p + 1) && *(p + 2)) {
    a = 1;
  }

  for (int i = 0; *p && *(p + 1); ++i) {
    a = 2;
  }

// CHECK: Function: f27
// CHECK: Block: B13 (Entry), Pred: Succ: B12

// CHECK: Block: B12, Pred: B13, Succ: B11
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK: Block: B11, Pred: B7, B12, Succ: B10, B6
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B10, Pred: B11, Succ: B9, B6
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B9, Pred: B10, Succ: B8, B6
// CHECK:   Widened bounds before stmt: *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B8, Pred: B9, Succ: B7
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 2 + 1)

// CHECK: Block: B7, Pred: B8, Succ: B11

// CHECK: Block: B6, Pred: B9, B10, B11, Succ: B5
// CHECK:   Widened bounds before stmt: int i = 0;
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B2, B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B5
// CHECK:   Widened bounds before stmt: ++i
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B1, Pred: B4, B5, Succ: B0
}
