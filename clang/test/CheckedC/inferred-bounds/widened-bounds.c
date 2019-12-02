// Tests for datafow analysis for bounds widening of _Nt_array_ptr's.
//
// RUN: %clang_cc1 -fdump-widened-bounds %s 2>1 | FileCheck %s

_Nt_array_ptr<char> p : count(0);
_Nt_array_ptr<char> q : count(0);
int a;

void f1() {
  if (*p) {}

// CHECK: In function: f1
// CHECK: [B1]
// CHECK: upper_bound(p) = 1
}

void f2() {
  if (*p) {
    a = 1;
    if (*(p + 1)) {
      a = 2;
      if (*(p + 2)) {
        a = 3;
      }
    }
  }

// CHECK: In function: f2
// CHECK: [B3]
// CHECK:    1: a = 1
// CHECK: upper_bound(p) = 1

// CHECK: [B2]
// CHECK:    1: a = 2
// CHECK: upper_bound(p) = 2

// CHECK: [B1]
// CHECK:    1: a = 3
// CHECK: upper_bound(p) = 3
}

void f3() {
  if (*(p + 2)) {
    a = 1;
    if (*(p + 1)) {
      a = 2;
      if (*p) {
        a = 3;
      }
    }
  }

// CHECK: In function: f3
// CHECK: [B3]
// CHECK:    1: a = 1
// CHECK: upper_bound(p) = 3

// CHECK: [B2]
// CHECK:    1: a = 2
// CHECK: upper_bound(p) = 3

// CHECK: [B1]
// CHECK:    1: a = 3
// CHECK: upper_bound(p) = 3
}

void f4() {
  if (*p) {
    a = 1;
    if (*q) {
      a = 2;
      if (*(p + 1)) {
        a = 3;
        if (*(q + 1)) {
          a = 4;
        }
      }
    }
  }

// CHECK: In function: f4
// CHECK: [B4]
// CHECK:    1: a = 1
// CHECK: upper_bound(p) = 1

// CHECK: [B3]
// CHECK:    1: a = 2
// CHECK: upper_bound(p) = 1
// CHECK: upper_bound(q) = 1

// CHECK: [B2]
// CHECK:    1: a = 3
// CHECK: upper_bound(p) = 2
// CHECK: upper_bound(q) = 1

// CHECK: [B1]
// CHECK:    1: a = 4
// CHECK: upper_bound(p) = 2
// CHECK: upper_bound(q) = 2
}

void f5() {
  if (*p) {
    a = 1;
    if (a) {
      a = 2;
      if (a) {
        a = 3;
        if (*(p + 1)) {
          a = 4;
        }
      }
    }
  }

// CHECK: In function: f5
// CHECK: [B4]
// CHECK:    1: a = 1
// CHECK: upper_bound(p) = 1

// CHECK: [B3]
// CHECK:    1: a = 2
// CHECK: upper_bound(p) = 1

// CHECK: [B2]
// CHECK:    1: a = 3
// CHECK: upper_bound(p) = 1

// CHECK: [B1]
// CHECK:    1: a = 4
// CHECK: upper_bound(p) = 2
}

void f6() {
  if (*p) {
    a = 1;
    if (*(p + 1))
      a = 2;
    else if (a == 3) {
      a = 4;
      if (*(p + 2))
        a = 5;
      else if (a == 6)
        a = 7;
    }
  }

// CHECK: In function: f6
// CHECK: [B7]
// CHECK:   1: a = 1
// CHECK: upper_bound(p) = 1

// CHECK: [B6]
// CHECK:   1: a = 2
// CHECK: upper_bound(p) = 2

// CHECK: [B5]
// CHECK:   1: a == 3
// CHECK: upper_bound(p) = 1

// CHECK: [B4]
// CHECK:   1: a = 4
// CHECK: upper_bound(p) = 1

// CHECK: [B3]
// CHECK:   1: a = 5
// CHECK: upper_bound(p) = 3

// CHECK: [B2]
// CHECK:   1: a == 6
// CHECK: upper_bound(p) = 1

// CHECK: [B1]
// CHECK:   1: a = 7
// CHECK: upper_bound(p) = 1
}

void f7() {
  if (*p) {
    if (a)
      a = 1;
  }

// CHECK: In function: f7
// CHECK: [B5]
// CHECK:   1: a
// CHECK: upper_bound(p) = 1

// CHECK: [B4]
// CHECK:   1: a = 1
// CHECK: upper_bound(p) = 1

  if (*p) {
    p = "a";
    if (a)
      a = 2;
  }

// CHECK: [B2]
// CHECK:   1: p = "a"
// CHECK:   2: a
// CHECK: upper_bound(p) = 1

// CHECK: [B1]
// CHECK:   1: a = 2
}

void f8() {
  if (*p && *q)
    a = 1;

// CHECK: In function: f8
// CHECK:  [B1]
// CHECK:    1: a = 1
// CHECK: upper_bound(p) = 1
// CHECK: upper_bound(q) = 1
}

void f9() {
  if (*p && *(p + 1) && *(p + 2))
    a = 1;

// check: in function: f9
// check:  [b1]
// check:    1: a = 1
// check: upper_bound(p) = 3
}

void f10() {
  if (*(p + 2) && *(p + 1) && *p)
    a = 1;

// CHECK: In function: f10
// CHECK:  [B1]
// CHECK:    1: a = 1
// CHECK: upper_bound(p) = 3
}

void f11() {
  if (*p && *(q + 10) && *(p + 5) && *(q + 3))
    a = 1;

// CHECK: In function: f11
// CHECK:  [B1]
// CHECK:    1: a = 1
// CHECK: upper_bound(p) = 6
// CHECK: upper_bound(q) = 11
}
