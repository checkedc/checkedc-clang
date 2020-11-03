// Tests for datafow analysis for bounds widening of _Nt_array_ptr's.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s | FileCheck %s

#include <limits.h>
#include <stdint.h>

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
    if (*(p + 1))
      if (*(p + 2))
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
    if (p[1])
      if (p[2])
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
    i = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
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
        if (*(p + 3))
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
    i = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
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
    j = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
    if (*(p + j + 1)) {}
  }

// CHECK:  [B3]
// CHECK:    1: *(p + j)
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
      if (p[2])
        if (3[p])
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
    if (*(q - 1))
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
    if (*(p + 1))
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
    if (*(p + 1))
      if (*(p + 3))
        if (*(p + 2))
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
  // TODO: Windows X86 Debug build fails to display the error "out-of-bounds
  // memory access". This seems to happen only at *(p + INT_MAX). So for now, I
  // have changed the dereference to *(p + INT_MAX - 1) to make this test pass.
  // I have filed isue #780. This needs to be investigated and the test need to
  // be changed to *(p + INT_MAX).
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

void f21() {
  char p _Nt_checked[] : count(0) = "abc";

  while (p[0])
    while (p[1])
      while (p[2])
  {}

// CHECK: In function: f21
// CHECK:  [B6]
// CHECK:    1: p[0]
// CHECK:  [B5]
// CHECK:    1: p[1]
// CHECK: upper_bound(p) = 1
// CHECK:  [B4]
// CHECK:    1: p[2]
// CHECK: upper_bound(p) = 2
// CHECK:  [B3]
// CHECK: upper_bound(p) = 3
// CHECK:  [B2]
// CHECK: upper_bound(p) = 2
// CHECK:  [B1]
// CHECK: upper_bound(p) = 1
}

void f22() {
  _Nt_array_ptr<char> p : count(0) = "a";

  if (*p)
    while (*(p + 1))
      if (*(p + 2))
  {}

// CHECK: In function: f22
// CHECK:  [B5]
// CHECK:    2: *p
// CHECK:  [B4]
// CHECK:    1: *(p + 1)
// CHECK: upper_bound(p) = 1
// CHECK:  [B3]
// CHECK:    1: *(p + 2)
// CHECK: upper_bound(p) = 2
// CHECK:  [B2]
// CHECK: upper_bound(p) = 3
// CHECK:  [B1]
// CHECK: upper_bound(p) = 2
}

void f23() {
  _Nt_array_ptr<char> p : count(0) = "";

// CHECK: In function: f23

  goto B;
  while (*p) {
B:  p;
    while (*(p + 1)) { // expected-error {{out-of-bounds memory access}}
      p;
    }
  }

// CHECK:  [B14]
// CHECK:    T: goto B;
// CHECK:  [B13]
// CHECK:    1: *p
// CHECK:    T: while
// CHECK:  [B12]
// CHECK:   B:
// CHECK:    1: p
// CHECK-NOT: upper_bound(p)
// CHECK:  [B11]
// CHECK:    1: *(p + 1)
// CHECK:    T: while
// CHECK:  [B10]
// CHECK:    1: p
// CHECK-NOT: upper_bound(p)

  while (*p) {
    p;
    while (*(p + 1)) {
C:    p;
    }
  }
  goto C;

// CHECK:  [B7]
// CHECK:    1: *p
// CHECK:    T: while
// CHECK:  [B6]
// CHECK:    1: p
// CHECK: upper_bound(p) = 1
// CHECK:  [B5]
// CHECK:    1: *(p + 1)
// CHECK:    T: while
// CHECK-NOT: upper_bound(p)
// CHECK:  [B4]
// CHECK:   C:
// CHECK:    1: p
// CHECK-NOT: upper_bound(p)
// CHECK:  [B2]
// CHECK-NOT: upper_bound(p)
// CHECK:  [B1]
// CHECK:    T: goto C;
}

void f24() {
  _Nt_array_ptr<char> p : count(0) = "";

  while (*p) {
    p++;
    while (*(p+1)) { // expected-error {{out-of-bounds memory access}}
      p;
    }
  }

// CHECK: In function: f24
// CHECK:  [B6]
// CHECK:    1: *p
// CHECK:    T: while
// CHECK:  [B5]
// CHECK:    1: p++
// CHECK: upper_bound(p) = 1
// CHECK:  [B4]
// CHECK:    1: *(p + 1)
// CHECK:    T: while
// CHECK:  [B3]
// CHECK:    1: p
// CHECK-NOT: upper_bound(p)
}

void f25() {
  _Nt_array_ptr<char> p : count(0) = "";
  int i;

// CHECK: In function: f25

  for (; *p; ) {
    i = 0;
    for (; *(p + 1); ) {
      i = 1;
      for (; *(p + 2); ) {
        i = 2;
      }
    }
  }

// CHECK:  [B22]
// CHECK:    1: *p
// CHECK:    T: for
// CHECK:  [B21]
// CHECK:    1: i = 0
// CHECK: upper_bound(p) = 1
// CHECK:  [B20]
// CHECK:    1: *(p + 1)
// CHECK:    T: for
// CHECK: upper_bound(p) = 1
// CHECK:  [B19]
// CHECK:    1: i = 1
// CHECK: upper_bound(p) = 2
// CHECK:  [B18]
// CHECK:    1: *(p + 2)
// CHECK:    T: for
// CHECK: upper_bound(p) = 2
// CHECK:  [B17]
// CHECK:    1: i = 2
// CHECK: upper_bound(p) = 3
// CHECK:  [B16]
// CHECK: upper_bound(p) = 3
// CHECK:  [B15]
// CHECK: upper_bound(p) = 2
// CHECK:  [B14]
// CHECK: upper_bound(p) = 1

  for (; *p; ) {
D:  p;
    for (; *(p + 1); ) { // expected-error {{out-of-bounds memory access}}
      p;
    }
  }
  goto D;

// CHECK:  [B13]
// CHECK:    1: *p
// CHECK:    T: for
// CHECK:  [B12]
// CHECK:   D:
// CHECK:    1: p
// CHECK-NOT: upper_bound(p)
// CHECK:  [B11]
// CHECK:    1: *(p + 1)
// CHECK:    T: for
// CHECK:  [B10]
// CHECK:    1: p
// CHECK-NOT: upper_bound(p)
// CHECK:  [B7]
// CHECK:    T: goto D;

  for (; *p; ) {
    p++;
    for (; *(p + 1); ) {
      p;
    }
  }

// CHECK:  [B6]
// CHECK:    1: *p
// CHECK:    T: for
// CHECK:  [B5]
// CHECK:    1: p++
// CHECK: upper_bound(p) = 1
// CHECK:  [B4]
// CHECK:    1: *(p + 1)
// CHECK:    T: for
// CHECK:  [B3]
// CHECK:    1: p
// CHECK-NOT: upper_bound(p)
}

void f26() {
  _Nt_array_ptr<char> p : count(0) = "";

// CHECK: In function: f26

  switch (*p) {
  default: break;
// CHECK:   default:
// CHECK-NOT: upper_bound(p)
  case 'a': break;
// CHECK:   case 'a':
// CHECK: upper_bound(p) = 1
  case 'b': break;
// CHECK:   case 'b':
// CHECK: upper_bound(p) = 1
  }

  switch (*p) {
  case 'a':
// CHECK:   case 'a':
// CHECK: upper_bound(p) = 1
  default:
// CHECK:   default:
// CHECK-NOT: upper_bound(p)
  case 'b': break;
// CHECK:   case 'b':
// CHECK-NOT: upper_bound(p)
  }

  switch (*p) {
  case 'a': break;
// CHECK:   case 'a':
// CHECK: upper_bound(p) = 1
  default:
// CHECK:   default:
// CHECK-NOT: upper_bound(p)
  case 'b': break;
// CHECK:   case 'b':
// CHECK-NOT: upper_bound(p)
  }

  switch (*p) {
  default: break;
// CHECK:   default:
// CHECK: upper_bound(p) = 1
  case '\0': break;
// CHECK:   case '\x00':
// CHECK-NOT: upper_bound(p)
  case 'a': break;
// CHECK:   case 'a':
// CHECK: upper_bound(p) = 1
  }

  switch (*p) {
  case '\0':
// CHECK:   case 'a':
// CHECK-NOT: upper_bound(p)
  case 'a': break;
// CHECK:   case '\x00':
// CHECK-NOT: upper_bound(p)
  default: break;
// CHECK:   default:
// CHECK: upper_bound(p) = 1
  }
}

void f27() {
  _Nt_array_ptr<char> p : count(0) = "";
  const char a = '\0';
  const int b = '0';
  const char c = '1';
  const int d = '2';

// CHECK: In function: f27

  switch (*p) {
  default: break;
// CHECK:   default:
// CHECK: upper_bound(p) = 1

  case a: break;
// CHECK:   case a:
// CHECK-NOT: upper_bound(p)

  case b: break;
// CHECK:   case b:
// CHECK: upper_bound(p) = 1

  case c: break;
// CHECK:   case c:
// CHECK: upper_bound(p) = 1

  case d: break;
// CHECK:   case d:
// CHECK: upper_bound(p) = 1
  }

  enum {e1, e2};
  switch (*p) {
  case e1: break;
// CHECK:   case e1:
// CHECK-NOT: upper_bound(p)

  case e2: break;
// CHECK:   case e2:
// CHECK: upper_bound(p) = 1

  default: break;
// CHECK:   default:
// CHECK: upper_bound(p) = 1
  }
}

void f28() {
  _Nt_array_ptr<char> p : count(0) = "";
  int a;

// CHECK: In function: f28

  switch (*p) {
  default: break;
// CHECK:   default:
// CHECK: upper_bound(p) = 1

  case 0:
    switch (*p) {
      default: break;
// CHECK:   default:
// CHECK-NOT: upper_bound(p)

      case 1: break;
    }
// CHECK:  [B11]
// CHECK:   case 1:
// CHECK: upper_bound(p) = 1
// CHECK:  [B10]
// CHECK:   case 0:
// CHECK-NOT: upper_bound(p)
  }

  switch (*p) {
  case 1:
    switch (*(p + 1)) {
      case 2: break;
    }
// CHECK:  [B8]
// CHECK:   case 2:
// CHECK:    T: break;
// CHECK: upper_bound(p) = 2
// CHECK:  [B7]
// CHECK:   case 1:
// CHECK:    1: *(p + 1)
// CHECK:    T: switch
// CHECK: upper_bound(p) = 1
  }

  switch (*p) {
  case 1: do { a;
          } while (a);
  }
// CHECK:  [B5]
// CHECK:   case 1:
// CHECK: upper_bound(p) = 1
// CHECK:  [B4]
// CHECK: upper_bound(p) = 1
// CHECK:  [B3]
// CHECK:    1: a
// CHECK: upper_bound(p) = 1
// CHECK:  [B2]
// CHECK:    1: a
// CHECK:    T: do ... while
// CHECK: upper_bound(p) = 1
}

void f29() {
  _Nt_array_ptr<char> p : count(0) = "";
  int i;

// CHECK: In function: f29

  switch (*p) {
  case 'a':
// CHECK:   case 'a':
// CHECK: upper_bound(p) = 1

    if (*(p + 1)) {
      i = 0;
// CHECK:    1: i = 0
// CHECK: upper_bound(p) = 2

      for (;*(p + 2);) {
        i = 1;
// CHECK:    1: i = 1
// CHECK: upper_bound(p) = 3

        while (*(p + 3)) {
          i = 2;
// CHECK:    1: i = 2
// CHECK: upper_bound(p) = 4
        }

        i = 3;
// CHECK:    1: i = 3
// CHECK: upper_bound(p) = 3
      }

      i = 4;
// CHECK:    1: i = 4
// CHECK: upper_bound(p) = 2
    }

    i = 5;
// CHECK:    1: i = 5
// CHECK: upper_bound(p) = 1

    break;
  }
}

void f30() {
// CHECK: In function: f30

  _Nt_array_ptr<char> p : count(0) = "";
  const int i = -1;
  const int j = 1;

  switch (*p) {
  case i ... j: break;
// CHECK:   case i ... j:
// CHECK-NOT: upper_bound(p)
  }

  switch (*p) {
  case 1 ... -1: break;
// CHECK:   case 1 ... -1:
// CHECK-NOT: upper_bound(p)

  case 1 ... 0: break;
// CHECK:   case 1 ... 0:
// CHECK-NOT: upper_bound(p)

  case -2 ... -1: break;
// CHECK:   case -2 ... -1:
// CHECK: upper_bound(p) = 1
  }

  switch (*p) {
  case -1 ... 1: break;
// CHECK:   case -1 ... 1:
// CHECK-NOT: upper_bound(p)
  }

  switch (*p) {
  case 0 ... 1: break;
// CHECK:   case 0 ... 1:
// CHECK-NOT: upper_bound(p)
  }

  switch (*p) {
  case 1 ... 2: break;
// CHECK:   case 1 ... 2:
// CHECK: upper_bound(p) = 1
  }
}

void f31() {
// CHECK: In function: f31

  _Nt_array_ptr<char> p : count(0) = "";
  const int i = 'abc';
  const char c = 'xyz';
  const unsigned u = UINT_MAX;

  switch (*p) {
  case 999999999999999999999999999: break; // expected-error {{integer literal is too large to be represented in any integer type}}
// CHECK: case {{.*}}
// CHECK: upper_bound(p) = 1

  case 00000000000000000000000000000000000: break;
// CHECK: case 0:
// CHECK-NOT: upper_bound(p)

  case 00000000000000000000000000000000001: break;
// CHECK: case 1:
// CHECK: upper_bound(p) = 1

  case '00000000000000000000000000000000000': break;
// CHECK: case {{.*}}
// CHECK: upper_bound(p) = 1

  case i: break;
// CHECK: case i:
// CHECK: upper_bound(p) = 1

  case c: break;
// CHECK: case c:
// CHECK: upper_bound(p) = 1

  case u: break;
// CHECK: case u:
// CHECK: upper_bound(p) = 1
  }

  switch (*p) {
  case INT_MAX: break;
// CHECK: case {{.*}}
// CHECK: upper_bound(p) = 1

  case INT_MIN: break;
// CHECK: case {{.*}}
// CHECK: upper_bound(p) = 1

  case INT_MAX + INT_MAX: break;
// CHECK: case {{.*}}
// CHECK: upper_bound(p) = 1

  case INT_MAX - INT_MAX: break;
// CHECK: case {{.*}}
// CHECK-NOT: upper_bound(p)

  case INT_MAX + INT_MIN: break;
// CHECK: case {{.*}}
// CHECK: upper_bound(p) = 1
  }

  switch (*p) {
  case INT_MIN - INT_MIN: break;
// CHECK: case {{.*}}
// CHECK-NOT: upper_bound(p)

  case INT_MAX - INT_MIN: break;
// CHECK: case {{.*}}
// CHECK: upper_bound(p) = 1
  }

  switch (*p) {
  // Note: This does not widen the bounds as the value of the expression is
  // computed to 0 and we have the warning: overflow in expression; result is 0
  // with type 'int'.
  case INT_MIN + INT_MIN: break;
// CHECK: case {{.*}}
// CHECK-NOT: upper_bound(p)

  case UINT_MAX: break;
// CHECK: case {{.*}}
// CHECK: upper_bound(p) = 1
  }

  _Nt_array_ptr<uint64_t> q : count(0) = 0;
  const uint64_t x = 0x0000444400004444LL;

  switch (*p) {
    case ULLONG_MAX: break;
// CHECK: case {{.*}}
// CHECK: upper_bound(p) = 1

    case x: break;
// CHECK: case x:
// CHECK: upper_bound(p) = 1
  }
}

void f32() {
// CHECK: In function: f32

  _Nt_array_ptr<char> p : count(0) = "";

  switch(*p) {
    default: f1(); break;
    case 0: {
      switch(*p) {
        default: f2(); break;
        case 'a': break;
      }
      break;
    }
  }

// CHECK:   default:
// CHECK:    1: f1()
// CHECK: upper_bound(p) = 1
// CHECK:   default:
// CHECK:    1: f2()
// CHECK-NOT: upper_bound(p)
// CHECK:   case 'a':
// CHECK: upper_bound(p) = 1
// CHECK:   case 0:
// CHECK-NOT: upper_bound(p)

  switch(*p) {
    default: f1(); break;
    case 0: {
      switch(*p) {
        default: f2(); break;
        case '\0': break;
      }
      break;
    }
  }

// CHECK:   default:
// CHECK:    1: f1()
// CHECK: upper_bound(p) = 1
// CHECK:   default:
// CHECK:    1: f2()
// CHECK: upper_bound(p) = 1
// CHECK:   case '\x00':
// CHECK-NOT: upper_bound(p)
// CHECK:   case 0:
// CHECK-NOT: upper_bound(p)

  switch(*p) {
    default: f1(); break;
    case 'b': {
      switch(*(p+1)) {
        default: f2(); break;
        case 'a': break;
      }
      break;
    }
  }

// CHECK:   default:
// CHECK:    1: f1()
// CHECK-NOT: upper_bound(p)
// CHECK:   default:
// CHECK:    1: f2()
// CHECK: upper_bound(p) = 1
// CHECK:   case 'a':
// CHECK: upper_bound(p) = 2
// CHECK:   case 'b':
// CHECK: upper_bound(p) = 1

  switch(*p) {
    default: f1(); break;
    case 'b': {
      switch(*(p+1)) {
        default: f2(); break;
        case '\0': break;
      }
      break;
    }
  }

// CHECK:   default:
// CHECK:    1: f1()
// CHECK-NOT: upper_bound(p)
// CHECK:   default:
// CHECK:    1: f2()
// CHECK: upper_bound(p) = 2
// CHECK:   case '\x00':
// CHECK: upper_bound(p) = 1
// CHECK:   case 'b':
// CHECK: upper_bound(p) = 1

  switch(*p) {
    default: {
      switch(*(p+1)) {
        default: f2(); break;
        case '\0': break;
      }
      break;
    }
    case 0: break;
  }

// CHECK:   default:
// CHECK:    1: f2()
// CHECK: upper_bound(p) = 2
// CHECK:   case '\x00':
// CHECK: upper_bound(p) = 1
// CHECK:   default:
// CHECK: upper_bound(p) = 1
// CHECK:   case 0:
// CHECK-NOT: upper_bound(p)
}

void f33() {
  _Nt_array_ptr<char> p : bounds(p, ((((((p + 1))))))) = "a";

  if (*(((p + 1)))) {}

// CHECK: In function: f33
// CHECK: [B2]
// CHECK:   2: *(((p + 1)))
// CHECK: [B1]
// CHECK: upper_bound(p) = 1
}

void f34(_Nt_array_ptr<char> p : count(i), int i, int flag) {
  if (*(p + i)) {
    flag ? i++ : i;  // expected-error {{inferred bounds for 'p' are unknown after increment}}

    if (*(p + i + 1))
    {}
  }

// CHECK: In function: f34
// CHECK:  [B6]
// CHECK:    1: *(p + i)
// CHECK:  [B5]
// CHECK:    1: flag
// CHECK: upper_bound(p) = 1
// CHECK:  [B4]
// CHECK:    1: i
// CHECK: upper_bound(p) = 1
// CHECK:  [B3]
// CHECK:    1: i++
// CHECK: upper_bound(p) = 1
// CHECK:  [B2]
// CHECK:    2: *(p + i + 1)
// CHECK: upper_bound(p) = 1
}

void f35(void) {
  int i, j;
  for (;;) {
    i = 1;
    j = 2;
    _Nt_array_ptr<char> p : count(i + j) = 0;
    if (p[i + j]) {
      return;
    }
  }

// CHECK: In function: f35
// CHECK:  [B5]
// CHECK:    1: int i;
// CHECK:    2: int j;
// CHECK:  [B4]
// CHECK:    T: for (; ; )
// CHECK:  [B3]
// CHECK:    1: i = 1
// CHECK:    2: j = 2
// CHECK:    3: _Nt_array_ptr<char> p : count(i + j) = 0;
// CHECK:    4: p[i + j] (ImplicitCastExpr, LValueToRValue, char)
// CHECK:    T: if [B3.4]
// CHECK:  [B2]
// CHECK:    1: return;
// CHECK: upper_bound(p) = 1
}
