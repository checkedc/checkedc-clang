// Tests for bounds widening of null-terminated arrays using the preorder AST
// to semantically compare two expressions.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify \
// RUN: -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s \
// RUN: | FileCheck %s

int a;

void f1(int i) {
  _Nt_array_ptr<char> p : bounds(p, p + i) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(i + p)) {
    a = 1;
  }

// CHECK: Function: f1
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(i + p)
// CHECK:     p: bounds(p, p + i)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, i + p + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f2(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + (i + j)) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(p + (j + i))) {
    a = 1;
  }

// CHECK: Function: f2
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + (i + j)) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + (j + i))
// CHECK:     p: bounds(p, p + (i + j))

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + (j + i) + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f3(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + (i * j)) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(p + (j * i))) {
    a = 1;
  }

// CHECK: Function: f3
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + (i * j)) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + (j * i))
// CHECK:     p: bounds(p, p + (i * j))

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + (j * i) + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f4(int i, int j, int k, int m, int n) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j + k + m + n) = "a";

  if (*(n + m + k + j + i + p)) {
    a = 1;
  }

// CHECK: Function: f4
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i + j + k + m + n) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(n + m + k + j + i + p)
// CHECK:     p: bounds(p, p + i + j + k + m + n)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, n + m + k + j + i + p + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f5(int i, int j, int k, int m, int n) {
  _Nt_array_ptr<char> p : bounds(p, (p + i) + (j + k) + (m + n)) = "a";

  if (*((n + m + k) + (j + i + p))) {
    a = 1;
  }

// CHECK: Function: f5
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, (p + i) + (j + k) + (m + n)) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *((n + m + k) + (j + i + p))
// CHECK:     p: bounds(p, (p + i) + (j + k) + (m + n))

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, (n + m + k) + (j + i + p) + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f6(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j + i + j) = "a";

  if (*(j + j + p + i + i)) {
    a = 1;
  }

// CHECK: Function: f6
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i + j + i + j) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(j + j + p + i + i)
// CHECK:     p: bounds(p, p + i + j + i + j)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, j + j + p + i + i + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f7(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i * j) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(p + i + j)) {
    a = 1;
  }

// CHECK: Function: f7
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i * j) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + i + j)
// CHECK:     p: bounds(p, p + i * j)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + i * j)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f8(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";

  if (*(p + i + i)) {
    a = 1;
  }

// CHECK: Function: f8
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + i + i)
// CHECK:     p: bounds(p, p + i + j)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + i + j)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f9(int i, int j, int k) {
  _Nt_array_ptr<char> p : bounds(p, (p + i) + (j * k)) = "a";

  if (*((p + i) + (j * k))) {
    a = 1;
  }

// CHECK: Function: f9
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, (p + i) + (j * k)) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *((p + i) + (j * k))
// CHECK:     p: bounds(p, (p + i) + (j * k))

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, (p + i) + (j * k) + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f10(int i, int j, int k) {
  _Nt_array_ptr<char> p : bounds(p, (p + i) + (j * k)) = "a";

  if (*((p + i) + (j + k))) {
    a = 1;
  }

// CHECK: Function: f10
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, (p + i) + (j * k)) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *((p + i) + (j + k))
// CHECK:     p: bounds(p, (p + i) + (j * k))

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, (p + i) + (j * k))

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f11(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";

  if (*(j + i + p)) {
    a = 1;
    if (*(i + j + p - 1 - -4 + -2)) {
      a = 2;
    }
  }

// CHECK: Function: f11
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i + j) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(j + i + p)
// CHECK:     p: bounds(p, p + i + j)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, j + i + p + 1)

// CHECK:   Widened bounds before stmt: *(i + j + p - 1 - -4 + -2)
// CHECK:     p: bounds(p, j + i + p + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, i + j + p - 1 - -4 + -2 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f12() {
  _Nt_array_ptr<char> p : bounds(p, p) = "a";

  if (*(p + 0)) {
    a = 1;
    if (*(p + 1 - 1 + -1 - +1 - -3)) {
      a = 2;
    }
  }

// CHECK: Function: f12
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + 0)
// CHECK:     p: bounds(p, p)

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK:   Widened bounds before stmt: *(p + 1 - 1 + -1 - +1 - -3)
// CHECK:     p: bounds(p, p + 0 + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + 1 - 1 + -1 - +1 - -3 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f13(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + (i * j * 2 + 2 + 1)) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(p + (i * j * 2 + 3))) {
    a = 1;
    if (*(p + (i * j * 2 + 1 + 1 + 1) + 1)) {
      a = 2;
    }
  }

// CHECK: Function: f13
// CHECK: Block: B5 (Entry), Pred: Succ: B4

// CHECK: Block: B4, Pred: B5, Succ: B3, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + (i * j * 2 + 2 + 1)) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + (i * j * 2 + 3))
// CHECK:     p: bounds(p, p + (i * j * 2 + 2 + 1))

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + (i * j * 2 + 3) + 1)

// CHECK:   Widened bounds before stmt: *(p + (i * j * 2 + 1 + 1 + 1) + 1)
// CHECK:     p: bounds(p, p + (i * j * 2 + 3) + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 2
// CHECK:     p: bounds(p, p + (i * j * 2 + 1 + 1 + 1) + 1 + 1)

// CHECK: Block: B1, Pred: B2, B3, B4, Succ: B0
}

void f14(int i) {
  _Nt_array_ptr<char> p : bounds(p, p + (i * 1)) = "a"; // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}}

  if (*(p + (i * 2))) {
    a = 1;
  }

// CHECK: Function: f14
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + (i * 1)) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + (i * 2))
// CHECK:     p: bounds(p, p + (i * 1))

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + (i * 1))

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f15(_Nt_array_ptr<char> p : count(6)) {
  if (*(p + (1 * (2 * 3)))) {
    a = 1;
  }

// CHECK: Function: f15
// CHECK: Block: B3 (Entry), Pred: Succ: B2

// CHECK: Block: B2, Pred: B3, Succ: B1, B0
// CHECK:   Widened bounds before stmt: *(p + (1 * (2 * 3)))
// CHECK:     p: bounds(p, p + 6)

// CHECK: Block: B1, Pred: B2, Succ: B0
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + (1 * (2 * 3)) + 1)
}

void f16(int i, int j) {
  _Nt_array_ptr<char> p : bounds(p, p + i + j + 8) = "a";
  if (*(p + (i + 3 * 2 + (j + 2)))) {
    a = 1;
  }

// CHECK: Function: f16
// CHECK: Block: B4 (Entry), Pred: Succ: B3

// CHECK: Block: B3, Pred: B4, Succ: B2, B1
// CHECK:   Widened bounds before stmt: _Nt_array_ptr<char> p : bounds(p, p + i + j + 8) = "a";
// CHECK:     <no widening>

// CHECK:   Widened bounds before stmt: *(p + (i + 3 * 2 + (j + 2)))
// CHECK:     p: bounds(p, p + i + j + 8)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p, p + (i + 3 * 2 + (j + 2)) + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
}

void f17(_Nt_array_ptr<char> p : bounds(p - 10, p - 3)) {
  if (p[-(1 + 2)]) {
    a = 1;
  }

// CHECK: Function: f17
// CHECK: Block: B3 (Entry), Pred: Succ: B2

// CHECK: Block: B2, Pred: B3, Succ: B1, B0
// CHECK:   Widened bounds before stmt: p[-(1 + 2)]
// CHECK:     p: bounds(p - 10, p - 3)

// CHECK: Block: B1, Pred: B2, Succ: B0
// CHECK:   Widened bounds before stmt: a = 1
// CHECK:     p: bounds(p - 10, p + -(1 + 2) + 1)
}
