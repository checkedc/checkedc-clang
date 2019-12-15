// Tests for datafow analysis for bounds widening of _Nt_array_ptr's.
//
// RUN: %clang_cc1 -fdump-widened-bounds %s 2>1 | FileCheck %s

_Nt_array_ptr<char> p : count(0);
_Nt_array_ptr<char> q : count(0);
int a;

void f1() {
  if (*p)
    if (*(p + 1))
      if (*(p + 2))
        if (*(p + 3))
          if (*(p + 4))
            if (*(p + 5))
              {}

// CHECK: In function: f1
// CHECK:  [B7]
// CHECK:    1: *p
// CHECK:  [B6]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B5]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B4]
// CHECK:    1: *(p + 3)
// CHECK: upper_bound(p) = 3
// CHECK:  [B3]
// CHECK:    1: *(p + 4)
// CHECK: upper_bound(p) = 4
// CHECK:  [B2]
// CHECK:    1: *(p + 5)
// CHECK: upper_bound(p) = 5
// CHECK:  [B1]
// CHECK: upper_bound(p) = 6
}

void f2() {
  if (*p)
    if (*(p + 1))
      if (*(p + 1))
        if (*(p + 2))
          if (*(p + 3))
            if (*p)
              {}

// CHECK: In function: f2
// CHECK:  [B7]
// CHECK:    1: *p
// CHECK:  [B6]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B5]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 2
// CHECK:  [B4]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B3]
// CHECK:    1: *(p + 3)
// CHECK: upper_bound(p) = 3
// CHECK:  [B2]
// CHECK:    1: *p
// CHECK: upper_bound(p) = 4
// CHECK:  [B1]
// CHECK: upper_bound(p) = 4
}

void f3() {
  if (*p)
    if (*(p + 5))
      if (*(p + 1))
        if (*(p + 7))
          if (*(p + 2))
            if (*(p + 9))
              {}

// CHECK: In function: f3
// CHECK:  [B7]
// CHECK:    1: *p
// CHECK:  [B6]
// CHECK:    1: *(p + 5)
// CHECK: upper_bound(p) = 1
// CHECK:  [B5]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B4]
// CHECK:    1: *(p + 7)
// CHECK: upper_bound(p) = 2
// CHECK:  [B3]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B2]
// CHECK:    1: *(p + 9)
// CHECK: upper_bound(p) = 3
// CHECK:  [B1]
// CHECK: upper_bound(p) = 3
}

void f4() {
  if (*p) {
    if (*(p + 1)) {
      if (*(p + 2)) {}
    }
    if (*(p + 1)) {
      if (*(p + 2)) {}
    }
    if (*(p + 1)) {
      if (*(p + 2)) {}
    }
  }

// CHECK: In function: f4
// CHECK:  [B10]
// CHECK:    1: *p
// CHECK:  [B9]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B8]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B7]
// CHECK: upper_bound(p) = 3
// CHECK:  [B6]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B5]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B4]
// CHECK: upper_bound(p) = 3
// CHECK:  [B3]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B2]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B1]
// CHECK: upper_bound(p) = 3
}

void f5() {
  if (*p) {
    if (*(p + 1)) {}
    else if (a) {
      if (*(p + 1)) {}
      else if (a) {}
    }
  }

// CHECK: In function: f5
// CHECK:  [B8]
// CHECK:    1: *p
// CHECK:  [B7]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B6]
// CHECK: upper_bound(p) = 2
// CHECK:  [B5]
// CHECK:    1: a
// CHECK: upper_bound(p) = 1
// CHECK:  [B4]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B3]
// CHECK: upper_bound(p) = 2
// CHECK:  [B2]
// CHECK:    1: a
// CHECK: upper_bound(p) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 1
}

void f9() {
  if (*p) {
    if (*q) {
      p = "a";
      if (*(p + 1)) {
        q = "b";
        if (*(q + 1)) {}
      }
    }
  }

// CHECK: In function: f9
// CHECK:  [B5]
// CHECK:    1: *p
// CHECK:  [B4]
// CHECK:    1: *q
// CHECK: upper_bound(p) = 1
// CHECK:  [B3]
// CHECK:    1: p = "a"
// CHECK:    2: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK: upper_bound(q) = 1
// CHECK:  [B2]
// CHECK:    1: q = "b"
// CHECK:    2: *(q + 1)
// CHECK:  [B1]
}

void f10() {
  if (!*p)
    if (*p) {}

// CHECK: In function: f10
// CHECK:  [B3]
// CHECK:    1: !*p
// CHECK:  [B2]
// CHECK:    1: *p
// CHECK:  [B1]
// CHECK: upper_bound(p) = 1
}

void f11() {
  if (*p)
    if (*(1 + p)) {}

// CHECK: In function: f11
// CHECK:  [B3]
// CHECK:   1: *p
// CHECK:  [B2]
// CHECK:    1: *(1 + p)
// CHECK: upper_bound(p) = 1
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
}
