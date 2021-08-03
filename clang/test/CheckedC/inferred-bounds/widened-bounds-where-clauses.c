// Tests for bounds widening of null-terminated arrays in presence of where
// clauses.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify \
// RUN: -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s \
// RUN: | FileCheck %s

int a;

void f1(_Nt_array_ptr<char> p : bounds(p, p + 1)) {
  if (*(p + 1)) {
    a = 1;
  }

  int x = 1 _Where p : bounds(p, p + x);

  if (*(p + 1)) {
    a = 2;
  }

  if (*(p + x)) {
    a = 3;
    if (*(p + x + 1)) {
      a = 4;
      if (*(p + x + 2)) {
        a = 5;
      }
    }
  }

// CHECK: Function: f1
// CHECK: Block: B10 (Entry), Pred: Succ: B9

// CHECK: Block: B9, Pred: B10, Succ: B8, B7
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B8, Pred: B9, Succ: B7
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B7, Pred: B8, B9, Succ: B6, B5
// CHECK:   Widened bounds before stmt: int x = 1;
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + x)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + x)

// CHECK: Block: B5, Pred: B6, B7, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *(p + x)
// CHECK:     p: bounds(p, p + x)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + x + 1)

// CHECK:   Widened bounds before stmt: *(p + x + 1)
// CHECK:     p: bounds(p, p + x + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + x + 1 + 1)

// CHECK:   Widened bounds before stmt: *(p + x + 2)
// CHECK:     p: bounds(p, p + x + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + x + 2 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, Succ: B0
}

void f2() {
  _Nt_array_ptr<char> p : bounds(p, p + 1) = "a";
  int x = 1 _Where p : count(x);

  if (*(p + 1)) {
    a = 1;
  } else if (*(p + x)) {
    a = 2;
  }

// CHECK: Function: f2
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B3
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + 1) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: int x = 1;
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + x)

// CHECK: Block: B4, Pred: B5, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + x)

// CHECK: Block: B3, Pred: B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + x)
// CHECK:     p: bounds(p, p + x)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + x + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f3(_Nt_array_ptr<char> p : bounds(p, p + 1)) {
  int x = 1 _Where p : bounds(p, p + x);

  if (*(p + x)) {
    x = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    if (*(p + x + 1)) {
      a = 1;
    }
  }

// CHECK: Function: f3
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: int x = 1;
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + x)
// CHECK:     p: bounds(p, p + x)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: x = 0
// CHECK:     p: bounds(p, p + x + 1)

// CHECK:   Widened bounds before stmt: *(p + x + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f4(_Nt_array_ptr<char> p : bounds(p, p + i), int i) {
  if (*(p + i)) {
    i = 0;  // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    if (*(p + i + 1)) {
      a = 1 _Where p : bounds(p, p + i);
      if (*(p + i)) {
        a = 2;
      }
    }
  }

// CHECK: Function: f4
// CHECK: Block: B7 (Entry), Pred: Succ: B6

// CHECK: Block: B6, Pred: B7, Succ: B5, B2
// CHECK:   Widened bounds before stmt: *(p + i)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B5, Pred: B6, Succ: B4, B2
// CHECK:   Widened bounds before stmt: i = 0
// CHECK:     p: bounds(p, p + i + 1)

// CHECK:   Widened bounds before stmt: *(p + i + 1)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B4, Pred: B5, Succ: B3, B2
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + i)

// CHECK:   Widened bounds before stmt: *(p + i)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + i + 1)

  if (*(p + i)) {
    a = 3;
  }

// CHECK: Block: B2, Pred: B3, B4, B5, B6, Succ: B1, B0
// CHECK:   Widened bounds before stmt: *(p + i)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B1, Pred: B2, Succ: B0
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + i + 1)
}

void f5() {
  _Nt_array_ptr<char> p : bounds(p, p + 1) = "a";
  _Nt_array_ptr<char> q : bounds(p, p + 1) = p;

  if (*(p + 1)) {
    a = 1;
  }

// CHECK: Function: f5
// CHECK: Block: B8 (Entry), Pred: Succ: B7

// CHECK: Block: B7, Pred: B8, Succ: B6, B5
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + 1) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : bounds(p, p + 1) = p;
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, p + 1)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(p, p + 1 + 1)

  a = 2 _Where q : bounds(q, q + 1);

  if (*(p + 1)) {
    a = 3;
  }

// CHECK: Block: B5, Pred: B6, B7, Succ: B4, B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, p + 1)
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(q, q + 1)

  if (*(q + 1)) {
    a = 4;
  }

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(q + 1)
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(p, q + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f6() {
  _Nt_array_ptr<char> p = "a";
  _Nt_array_ptr<char> q  = "b";
  _Nt_array_ptr<char> r  = "c";

  int x = 1 _Where p : bounds(p, p + x + 1) _And q : count(x) _And r : bounds(p, p + x);

  if (*(p + x) && *(p + x + 1) && *(q + x) && *(q + x + 1)) { // expected-error {{it is not possible to prove that the inferred bounds of 'r' imply the declared bounds of 'r' after statement}}
    a = 1;
  }

// CHECK: Function: f6
// CHECK: Block: B7 (Entry), Pred: Succ: B6

// CHECK: Block: B6, Pred: B7, Succ: B5, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> q : count(0) = "b";
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> r : count(0) = "c";
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK:   Widened bounds before stmt: int x = 1;
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)
// CHECK:     r: bounds(r, r + 0)

// CHECK:   Widened bounds before stmt: *(p + x)
// CHECK:     p: bounds(p, p + x + 1)
// CHECK:     q: bounds(q, q + x)
// CHECK:     r: bounds(p, p + x)

// CHECK: Block: B5, Pred: B6, Succ: B4, B1
// CHECK:   Widened bounds before stmt: *(p + x + 1)
// CHECK:     p: bounds(p, p + x + 1)
// CHECK:     q: bounds(q, q + x)
// CHECK:     r: bounds(r, p + x + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(q + x)
// CHECK:     p: bounds(p, p + x + 1 + 1)
// CHECK:     q: bounds(q, q + x)
// CHECK:     r: bounds(r, p + x + 1 + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(q + x + 1)
// CHECK:     p: bounds(p, p + x + 1 + 1)
// CHECK:     q: bounds(q, q + x + 1)
// CHECK:     r: bounds(r, p + x + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + x + 1 + 1)
// CHECK:     q: bounds(q, q + x + 1 + 1)
// CHECK:     r: bounds(r, p + x + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, B5, B6, Succ: B0
}

void f7() {
  _Nt_array_ptr<char> p = "a";

  int x = 1 _Where x > 0 _And p : count(x) _And 1 == 1 _And p : count(x + 10);

  if (*(p + x)) {
    a = 1;
  }

  if (*(p + x + 10)) {
    a = 2;
  }

// CHECK: Function: f7
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B6, Succ: B4, B3
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: int x = 1;
// CHECK:     p: bounds(p, p + 0)

// CHECK:   Widened bounds before stmt: *(p + x)
// CHECK:     p: bounds(p, p + x + 10)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + x + 10)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + x + 10)
// CHECK:     p: bounds(p, p + x + 10)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + x + 10 + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f8() {
  _Nt_array_ptr<char> p = "a";

  while (*p) {
    p = "b" _Where p : count(0);
  }

// CHECK: Function: f8
// CHECK: Block: B10 (Entry), Pred: Succ: B9

// CHECK: Block: B9, Pred: B10, Succ: B8
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : count(0) = "a";
// CHECK:     <no widening>

// CHECK: Block: B8, Pred: B6, B9, Succ: B7, B5
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B7, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: p = "b"
// CHECK:     p: bounds(p, p + 1)

  int x = 1 _Where p : bounds(p, p + x);

  while (*(p + x)) {
    x = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    a = 1 _Where p : bounds(p, p + x);
  }

// CHECK: Block: B6, Pred: B7, Succ: B8

// CHECK: Block: B5, Pred: B8, Succ: B4
// CHECK:   Widened bounds before stmt: int x = 1;
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B4, Pred: B2, B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + x)
// CHECK:     p: bounds(p, p + x)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: x = 0
// CHECK:     p: bounds(p, p + x + 1)

// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B2, Pred: B3, Succ: B4

// CHECK: Block: B1, Pred: B4, Succ: B0
}
