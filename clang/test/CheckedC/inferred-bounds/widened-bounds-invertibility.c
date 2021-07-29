// Tests for invertibility of statements in the datafow analysis for bounds
// widening of null-terminated arrays.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify \
// RUN: -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s \
// RUN: | FileCheck %s

// expected-no-diagnostics

#include <limits.h>
#include <stdint.h>

int a;

void f1(_Nt_array_ptr<char> p : count(len), unsigned len) {

  while (*(p + len)) {
    len++;
    a = 1;
  }

// CHECK: Function: f1
// CHECK: Block: B25 (Entry), Pred: Succ: B24

// CHECK: Block: B24, Pred: B22, B25, Succ: B23, B21
// CHECK:   Widened bounds before stmt: *(p + len)
// CHECK:     p: bounds(p, p + len)

// CHECK: Block: B23, Pred: B24, Succ: B22
// CHECK:   Widened bounds before stmt: len++
// CHECK:     p: bounds(p, p + len + 1)

// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + len - 1U + 1)

  while (*(p + len)) {
    ++len;
    a = 2;
  }

// CHECK: Block: B22, Pred: B23, Succ: B24

// CHECK: Block: B21, Pred: B19, B24, Succ: B20, B18
// CHECK:   Widened bounds before stmt: *(p + len)
// CHECK:     p: bounds(p, p + len)

// CHECK: Block: B20, Pred: B21, Succ: B19
// CHECK:   Widened bounds before stmt: ++len
// CHECK:     p: bounds(p, p + len + 1)

// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + len - 1U + 1)

  while (*(p + len)) {
    len--;
    a = 3;
  }

// CHECK: Block: B19, Pred: B20, Succ: B21

// CHECK: Block: B18, Pred: B16, B21, Succ: B17, B15
// CHECK:   Widened bounds before stmt: *(p + len)
// CHECK:     p: bounds(p, p + len)

// CHECK: Block: B17, Pred: B18, Succ: B16
// CHECK:   Widened bounds before stmt: len--
// CHECK:     p: bounds(p, p + len + 1)

// CHECK:   Widened bounds before stmt: a = 3
// CHECK:     p: bounds(p, p + len + 1U + 1)

  while (*(p + len)) {
    --len;
    a = 4;
  }

// CHECK: Block: B16, Pred: B17, Succ: B18

// CHECK: Block: B15, Pred: B13, B18, Succ: B14, B12
// CHECK:   Widened bounds before stmt: *(p + len)
// CHECK:     p: bounds(p, p + len)

// CHECK: Block: B14, Pred: B15, Succ: B13
// CHECK:   Widened bounds before stmt: --len
// CHECK:     p: bounds(p, p + len + 1)

// CHECK:   Widened bounds before stmt: a = 4
// CHECK:     p: bounds(p, p + len + 1U + 1)

  while (*(p + len)) {
    len = len + 1;
    a = 5;
  }

// CHECK: Block: B13, Pred: B14, Succ: B15

// CHECK: Block: B12, Pred: B10, B15, Succ: B11, B9
// CHECK:   Widened bounds before stmt: *(p + len)
// CHECK:     p: bounds(p, p + len)

// CHECK: Block: B11, Pred: B12, Succ: B10
// CHECK:   Widened bounds before stmt: len = len + 1
// CHECK:     p: bounds(p, p + len + 1)

// CHECK:   Widened bounds before stmt: a = 5
// CHECK:     p: bounds(p, p + len - 1 + 1)

  while (*(p + len)) {
    len = len - 1;
    a = 6;
  }

// CHECK: Block: B10, Pred: B11, Succ: B12

// CHECK: Block: B9, Pred: B7, B12, Succ: B8, B6
// CHECK:   Widened bounds before stmt: *(p + len)
// CHECK:     p: bounds(p, p + len)

// CHECK: Block: B8, Pred: B9, Succ: B7
// CHECK:   Widened bounds before stmt: len = len - 1
// CHECK:     p: bounds(p, p + len + 1)

// CHECK:   Widened bounds before stmt: a = 6
// CHECK:     p: bounds(p, p + len + 1 + 1)

  while (*(p + len)) {
    len += 1;
    a = 7;
  }

// CHECK: Block: B7, Pred: B8, Succ: B9

// CHECK: Block: B6, Pred: B4, B9, Succ: B5, B3
// CHECK:   Widened bounds before stmt: *(p + len)
// CHECK:     p: bounds(p, p + len)

// CHECK: Block: B5, Pred: B6, Succ: B4
// CHECK:   Widened bounds before stmt: len += 1
// CHECK:     p: bounds(p, p + len + 1)

// CHECK:   Widened bounds before stmt: a = 7
// CHECK:     p: bounds(p, p + len - 1 + 1)

  while (*(p + len)) {
    len -= 1;
    a = 8;
  }

// CHECK: Block: B4, Pred: B5, Succ: B6

// CHECK: Block: B3, Pred: B1, B6, Succ: B2, B0
// CHECK:   Widened bounds before stmt: *(p + len)
// CHECK:     p: bounds(p, p + len)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: len -= 1
// CHECK:     p: bounds(p, p + len + 1)

// CHECK:   Widened bounds before stmt: a = 8
// CHECK:     p: bounds(p, p + len + 1 + 1)
}

void f2(_Nt_array_ptr<char> p : count(len), unsigned len) {
  while (*(p + len)) {
    if (*(p + len + 1)) {
      if (*(p + len + 2)) {
        len = len + 1;    
        a = *(p + len + 1);
      }
    }
  }

// CHECK: Function: f2
// CHECK: Block: B6 (Entry), Pred: Succ: B5

// CHECK: Block: B5, Pred: B1, B6, Succ: B4, B0
// CHECK:   Widened bounds before stmt: *(p + len)
// CHECK:     p: bounds(p, p + len)

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: *(p + len + 1)
// CHECK:     p: bounds(p, p + len + 1)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: *(p + len + 2)
// CHECK:     p: bounds(p, p + len + 1 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: len = len + 1
// CHECK:     p: bounds(p, p + len + 2 + 1)

// CHECK:   Widened bounds before stmt: a = *(p + len + 1)
// CHECK:     p: bounds(p, p + len - 1 + 2 + 1)
}

void f3(_Nt_array_ptr<char> p : count(len), unsigned len) {
  len--;
  a = 1;

// CHECK: Function: f3
// CHECK: Block: B2 (Entry), Pred: Succ: B1

// CHECK: Block: B1, Pred: B2, Succ: B0
// CHECK:   Widened bounds before stmt: len--
// CHECK:     p: bounds(p, p + len)

// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + len + 1U)
}
