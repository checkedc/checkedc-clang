// Tests for datafow analysis for bounds widening of null-terminated arrays in
// presence of complex conditionals.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify \
// RUN: -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s \
// RUN: | FileCheck %s

// expected-no-diagnostics

#include <limits.h>
#include <stdint.h>

int a;

void f1(_Nt_array_ptr<char> p : count(0)) {

  if (*p) {
    a = 1;
  } else {
    a = 2;
  }

// CHECK: Function: f1
// CHECK: Block: B7 (Entry), Pred: Succ: B6

// CHECK: Block: B6, Pred: B7, Succ: B5, B4
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B5, Pred: B6, Succ: B3
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B6, Succ: B3
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)

  if (!*p) {
    a = 3;
  } else {
    a = 4;
  }

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: !*p
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B2, Pred: B3, Succ: B0
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)

// CHECK: Block: B1, Pred: B3, Succ: B0
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)
}

void f2(_Nt_array_ptr<char> p : count(i), int i) {

  if (*(p + i) == 0) {
    a = 1;
  } else {
    a = 2;
  }

// CHECK: Function: f2
// CHECK: Block: B13 (Entry), Pred: Succ: B12

// CHECK: Block: B12, Pred: B13, Succ: B11, B10
// CHECK:   Widened bounds before stmt: *(p + i) == 0
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B11, Pred: B12, Succ: B9
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B10, Pred: B12, Succ: B9
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + i + 1)

  if (*(i + p) != 0) {
    a = 3;
  } else {
    a = 4;
  }

// CHECK: Block: B9, Pred: B10, B11, Succ: B8, B7
// CHECK:   Widened bounds before stmt: *(i + p) != 0
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B8, Pred: B9, Succ: B6
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, i + p + 1)

// CHECK: Block: B7, Pred: B9, Succ: B6
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + i)

  if ('a' == *(p + i)) {
    a = 5;
  } else {
    a = 6;
  }

// CHECK: Block: B6, Pred: B7, B8, Succ: B5, B4
// CHECK:   Widened bounds before stmt: 'a' == *(p + i)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B5, Pred: B6, Succ: B3
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + i + 1)

// CHECK: Block: B4, Pred: B6, Succ: B3
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + i)

  if ('a' != *(i + p)) {
    a = 7;
  } else {
    a = 8;
  }

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: 'a' != *(i + p)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B2, Pred: B3, Succ: B0
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B1, Pred: B3, Succ: B0
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, i + p + 1)
}

void f3(_Nt_array_ptr<char> p : bounds(p, p)) {

  if (*p) {
    a = 1;
  } else if (*p == 'a') {
    a = 2;
  } else {
    a = 3;
  }

// CHECK: Function: f3
// CHECK: Block: B16 (Entry), Pred: Succ: B15

// CHECK: Block: B15, Pred: B16, Succ: B14, B13
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p)

// CHECK: Block: B14, Pred: B15, Succ: B10
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B13, Pred: B15, Succ: B12, B11
// CHECK:   Widened bounds before stmt: *p == 'a'
// CHECK:     p: bounds(p, p)

// CHECK: Block: B12, Pred: B13, Succ: B10
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B11, Pred: B13, Succ: B10
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p)

  if (!*p) {
    a = 4;
  } else if (!*p) {
    a = 5;
  } else {
    a = 6;
  }

// CHECK: Block: B10, Pred: B11, B12, B14, Succ: B9, B8
// CHECK:   Widened bounds before stmt: !*p
// CHECK:     p: bounds(p, p)

// CHECK: Block: B9, Pred: B10, Succ: B5
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p)

// CHECK: Block: B8, Pred: B10, Succ: B7, B6
// CHECK:   Widened bounds before stmt: !*p
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B7, Pred: B8, Succ: B5
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B6, Pred: B8, Succ: B5
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 1)

  if (!*p) {
    a = 7;
  } else if (*(p + 1) != '\0') {
    a = 8;
  } else {
    a = 9;
  }

// CHECK: Block: B5, Pred: B6, B7, B9, Succ: B4, B3
// CHECK:   Widened bounds before stmt: !*p
// CHECK:     p: bounds(p, p)

// CHECK: Block: B4, Pred: B5, Succ: B0
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p)

// CHECK: Block: B3, Pred: B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + 1) != '\x00'
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B0
// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + 1 + 1)

// CHECK: Block: B1, Pred: B3, Succ: B0
// CHECK:   Widened bounds before stmt: a = 9
// CHECK:     p: bounds(p, p + 1)
}

void f4(_Nt_array_ptr<char> p : bounds(p, p)) {
  if (!!*p) {
    a = 1;
  }

// CHECK: Function: f4
// CHECK: Block: B13 (Entry), Pred: Succ: B12

// CHECK: Block: B12, Pred: B13, Succ: B11, B10
// CHECK:   Widened bounds before stmt: !!*p
// CHECK:     p: bounds(p, p)

// CHECK: Block: B11, Pred: B12, Succ: B10
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1)

  if (!!!*p) {
    a = 2;
  }

// CHECK: Block: B10, Pred: B11, B12, Succ: B9, B8
// CHECK:   Widened bounds before stmt: !!!*p
// CHECK:     p: bounds(p, p)

// CHECK: Block: B9, Pred: B10, Succ: B8
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p)

  if (!(*p == 0)) {
    a = 3;
  }

// CHECK: Block: B8, Pred: B9, B10, Succ: B7, B6
// CHECK:   Widened bounds before stmt: !(*p == 0)
// CHECK:     p: bounds(p, p)

// CHECK: Block: B7, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1)

  if (!(*p != 0)) {
    a = 4;
  }

// CHECK: Block: B6, Pred: B7, B8, Succ: B5, B4
// CHECK:   Widened bounds before stmt: !(*p != 0)
// CHECK:     p: bounds(p, p)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p)

  if (!!(*p != 0)) {
    a = 5;
  }

// CHECK: Block: B4, Pred: B5, B6, Succ: B3, B2
// CHECK:   Widened bounds before stmt: !!(*p != 0)
// CHECK:     p: bounds(p, p)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 1)

  if (!!!(!*p == 0)) {
    a = 6;
  }

// CHECK: Block: B2, Pred: B3, B4, Succ: B1, B0
// CHECK:   Widened bounds before stmt: !!!(!*p == 0)
// CHECK:     p: bounds(p, p)

// CHECK: Block: B1, Pred: B2, Succ: B0
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p)
}

void f5(_Nt_array_ptr<char> p : bounds(p, p + 1)) {
  char c;

  if ((c = *(p + 1)) == 'a') {
    a = 1;
  }

// CHECK: Function: f5
// CHECK: Block: B10 (Entry), Pred: Succ: B9

// CHECK: Block: B9, Pred: B10, Succ: B8, B7
// CHECK:   Widened bounds before stmt: char c;
// CHECK:     p: bounds(p, p + 1)

// CHECK:   Widened bounds before stmt: (c = *(p + 1)) == 'a'
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B8, Pred: B9, Succ: B7
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)

  if (0 != (c = *(p + 1))) {
    a = 2;
  }

// CHECK: Block: B7, Pred: B8, B9, Succ: B6, B5
// CHECK:   Widened bounds before stmt: 0 != (c = *(p + 1))
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 + 1)

  if ((c = *(p + 1))) {
    a = 3;
  }

// CHECK: Block: B5, Pred: B6, B7, Succ: B4, B3
// CHECK:   Widened bounds before stmt: c = *(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1 + 1)

  if ((c = !*(p + 1))) {
    a = 4;
  }

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: c = !*(p + 1)
// CHECK:     p: bounds(p, p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)
}

void f6(_Nt_array_ptr<char> p : count(0),
        _Nt_array_ptr<char> q : count(0)) {

  if (*p == a) {
    a = 1;
  }

// CHECK: Function: f6
// CHECK: Block: B22 (Entry), Pred: Succ: B21

// CHECK: Block: B21, Pred: B22, Succ: B20, B19
// CHECK:   Widened bounds before stmt: *p == a
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B20, Pred: B21, Succ: B19
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

  if (*p == *q) {
    a = 2;
  }

// CHECK: Block: B19, Pred: B20, B21, Succ: B18, B17
// CHECK:   Widened bounds before stmt: *p == *q
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B18, Pred: B19, Succ: B17
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

  if ((*p == 'a') && (*q != 'b')) {
    a = 3;
  }

// CHECK: Block: B17, Pred: B18, B19, Succ: B16, B14
// CHECK:   Widened bounds before stmt: *p == 'a'
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B16, Pred: B17, Succ: B15, B14
// CHECK:   Widened bounds before stmt: *q != 'b'
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B15, Pred: B16, Succ: B14
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 0)

  if (*p && !*q) {
    a = 4;
  } else if (!*p && *q) {
    a = 5;
  }

// CHECK: Block: B14, Pred: B15, B16, B17, Succ: B13, B11
// CHECK:   Widened bounds before stmt: *p
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B13, Pred: B14, Succ: B12, B11
// CHECK:   Widened bounds before stmt: !*q
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B12, Pred: B13, Succ: B8
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B11, Pred: B13, B14, Succ: B10, B8
// CHECK:   Widened bounds before stmt: !*p
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B10, Pred: B11, Succ: B9, B8
// CHECK:   Widened bounds before stmt: *q
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B9, Pred: B10, Succ: B8
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)

  if (*p != 0 && *(p + 1) && 'a' == *(p + 2)) {
    a = 6;
  }

// CHECK: Block: B8, Pred: B9, B10, B11, B12, Succ: B7, B4
// CHECK:   Widened bounds before stmt: *p != 0
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B7, Pred: B8, Succ: B6, B4
// CHECK:   Widened bounds before stmt: *(p + 1)
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B6, Pred: B7, Succ: B5, B4
// CHECK:   Widened bounds before stmt: 'a' == *(p + 2)
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 2 + 1)
// CHECK:     q: bounds(q, q + 0)

  char c, d;
  if (((c = *p) != 0) && 'a' == (d = *q)) {
    a = 7;
  }

// CHECK: Block: B4, Pred: B5, B6, B7, B8, Succ: B3, B1
// CHECK:   Widened bounds before stmt: char c;
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK:   Widened bounds before stmt: char d;
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK:   Widened bounds before stmt: (c = *p) != 0
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: 'a' == (d = *q)
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 0)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + 1)
// CHECK:     q: bounds(q, q + 1)
}

void f7(char p _Nt_checked[1], char q _Nt_checked[2]) {
  if (p[0] != '\0' && 'a' == 1[q] && p[1] == 'b') {
    a = 1;
  }

// CHECK: Function: f7
// CHECK: Block: B17 (Entry), Pred: Succ: B16

// CHECK: Block: B16, Pred: B17, Succ: B15, B12
// CHECK:   Widened bounds before stmt: p[0] != '\x00'
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B15, Pred: B16, Succ: B14, B12
// CHECK:   Widened bounds before stmt: 'a' == 1[q]
// CHECK:     p: bounds(p, p + 0 + 1)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B14, Pred: B15, Succ: B13, B12
// CHECK:   Widened bounds before stmt: p[1] == 'b'
// CHECK:     p: bounds(p, p + 0 + 1)
// CHECK:     q: bounds(q, q + 1 + 1)

// CHECK: Block: B13, Pred: B14, Succ: B12
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 1 + 1)
// CHECK:     q: bounds(q, q + 1 + 1)

  char c;
  if ((c = q[0]) && (c = q[1])) {
    a = 2;
  }

// CHECK: Block: B12, Pred: B13, B14, B15, B16, Succ: B11, B9
// CHECK:   Widened bounds before stmt: char c;
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)

// CHECK:   Widened bounds before stmt: (c = q[0])
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B11, Pred: B12, Succ: B10, B9
// CHECK:   Widened bounds before stmt: (c = q[1])
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B10, Pred: B11, Succ: B9
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1 + 1)

  if (!p[0]) {
    a = 3;
  } else if (!q[1]) {
    a = 4;
  } else if (p[0]) {
    a = 5;
  } else if (q[1]) {
    a = 6;
  }

// CHECK: Block: B9, Pred: B10, B11, B12, Succ: B8, B7
// CHECK:   Widened bounds before stmt: !p[0]
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B8, Pred: B9, Succ: B1
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B7, Pred: B9, Succ: B6, B5
// CHECK:   Widened bounds before stmt: !q[1]
// CHECK:     p: bounds(p, p + 0 + 1)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B6, Pred: B7, Succ: B1
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 0 + 1)
// CHECK:     q: bounds(q, q + 1)

// CHECK: Block: B5, Pred: B7, Succ: B4, B3
// CHECK:   Widened bounds before stmt: p[0]
// CHECK:     p: bounds(p, p + 0 + 1)
// CHECK:     q: bounds(q, q + 1 + 1)

// CHECK: Block: B4, Pred: B5, Succ: B1
// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + 0 + 1)
// CHECK:     q: bounds(q, q + 1 + 1)

// CHECK: Block: B3, Pred: B5, Succ: B2, B1
// CHECK:   Widened bounds before stmt: q[1]
// CHECK:     p: bounds(p, p + 0 + 1)
// CHECK:     q: bounds(q, q + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + 0 + 1)
// CHECK:     q: bounds(q, q + 1 + 1)
}

void f8(_Nt_array_ptr<_Ptr<char>> p : count(0),
        _Nt_array_ptr<_Ptr<char>> q : bounds(*q, *q)) {

  if (**p) {
    a = 1;
  }

// CHECK: Function: f8
// CHECK: Block: B9 (Entry), Pred: Succ: B8

// CHECK: Block: B8, Pred: B9, Succ: B7, B6
// CHECK:   Widened bounds before stmt: **p
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(*q, *q)

// CHECK: Block: B7, Pred: B8, Succ: B6
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(*q, *q)

  if (!**p) {
    a = 2;
  }

// CHECK: Block: B6, Pred: B7, B8, Succ: B5, B4
// CHECK:   Widened bounds before stmt: !**p
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(*q, *q)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(*q, *q)

  if (**q) {
    a = 3;
  }

// CHECK: Block: B4, Pred: B5, B6, Succ: B3, B2
// CHECK:   Widened bounds before stmt: **q
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(*q, *q)

// CHECK: Block: B3, Pred: B4, Succ: B2
// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(*q, *q + 1)

  if (!**q) {
    a = 4;
  }

// CHECK: Block: B2, Pred: B3, B4, Succ: B1, B0
// CHECK:   Widened bounds before stmt: !**q
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(*q, *q)

// CHECK: Block: B1, Pred: B2, Succ: B0
// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + 0)
// CHECK:     q: bounds(*q, *q)
}
