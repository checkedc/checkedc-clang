// Tests for dumping the various datafow sets computed by the bounds widening
// analysis.
//
// RUN: %clang_cc1 -fdump-widened-bounds-dataflow-sets -verify \
// RUN: -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s \
// RUN: | FileCheck %s

int a;

void f1(_Nt_array_ptr<char> p : bounds(p, p + i), int i,
        _Nt_array_ptr<char> q : bounds(q, q + j), int j) {
  char r _Nt_checked[3] = "ab";

  if (*(p + i)) {
    if (*(p + i + 1)) {
      a = 1;
    }
  }

  if (*(q + j)) {
    a = 2;
  }

  a = 3 _Where r : bounds(r, r + 1);

  i++; // expected-error {{inferred bounds for 'p' are unknown after increment}}
  if (*(p + i)) {
    a = 4;
  }

// CHECK: Function: f1
// CHECK: Block: B9 (Entry), Pred: Succ: B8
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)

// CHECK: Block: B8, Pred: B9, Succ: B7, B5
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: Top
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)

// CHECK:   Stmt: char r_Nt_checked[3] = "ab";
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: Top
// CHECK:   Gen:
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Kill:
// CHECK:     r
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)

// CHECK:   Stmt: *(p + i)
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Gen:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:   Kill:
// CHECK:     p
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)

// CHECK: Block: B7, Pred: B8, Succ: B6, B5
// CHECK:   In:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1 + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)

// CHECK:   Stmt: *(p + i + 1)
// CHECK:   In:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Gen:
// CHECK:     p: bounds(p, p + i + 1 + 1)
// CHECK:   Kill:
// CHECK:     p
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1 + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)

// CHECK: Block: B6, Pred: B7, Succ: B5
// CHECK:   In:
// CHECK:     p: bounds(p, p + i + 1 + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1 + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)

// CHECK:   Stmt: a = 1
// CHECK:   In:
// CHECK:     p: bounds(p, p + i + 1 + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Gen:
// CHECK:     {}
// CHECK:   Kill:
// CHECK:     {}
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1 + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)

// CHECK: Block: B5, Pred: B6, B7, B8, Succ: B4, B3
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j + 1)
// CHECK:     r: bounds(r, r + 2)

// CHECK:   Stmt: *(q + j)
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Gen:
// CHECK:     q: bounds(q, q + j + 1)
// CHECK:   Kill:
// CHECK:     q
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j + 1)
// CHECK:     r: bounds(r, r + 2)

// CHECK: Block: B4, Pred: B5, Succ: B3
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j + 1)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j + 1)
// CHECK:     r: bounds(r, r + 2)

// CHECK:   Stmt: a = 2
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j + 1)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Gen:
// CHECK:     {}
// CHECK:   Kill:
// CHECK:     {}
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j + 1)
// CHECK:     r: bounds(r, r + 2)

// CHECK: Block: B3, Pred: B4, B5, Succ: B2, B1
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)

// CHECK:   Stmt: a = 3
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 2)
// CHECK:   Gen:
// CHECK:     r: bounds(r, r + 1)
// CHECK:   Kill:
// CHECK:     r
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)

// CHECK:   Stmt: i++
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)
// CHECK:   Gen:
// CHECK:     p: bounds(p, p + i)
// CHECK:   Kill:
// CHECK:     p
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)

// CHECK:   Stmt: *(p + i)
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)
// CHECK:   Gen:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:   Kill:
// CHECK:     p
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)

// CHECK: Block: B2, Pred: B3, Succ: B1
// CHECK:   In:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)

// CHECK:   Stmt: a = 4
// CHECK:   In:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)
// CHECK:   Gen:
// CHECK:     {}
// CHECK:   Kill:
// CHECK:     {}
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i + 1)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)

// CHECK: Block: B1, Pred: B2, B3, Succ: B0
// CHECK:   In:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)
// CHECK:   Out:
// CHECK:     p: bounds(p, p + i)
// CHECK:     q: bounds(q, q + j)
// CHECK:     r: bounds(r, r + 1)
}
