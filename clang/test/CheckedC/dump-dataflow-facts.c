// UNSUPPORTED: system-windows

// Tests for dumping of datafow analysis for collecting facts
//
// RUN: %clang_cc1 -fdump-extracted-comparison-facts %s 2>1 | FileCheck %s

//===================================================================
// Dumps of different kinds of collected dataflow facts
//===================================================================

// --- Testing Basic Functionalities --- //

int f(int a);
void fn_1(void) {
  int a, b, c;
  if (b < c)
    if (f((a=5)+3))
      b = c;

// CHECK-LABEL: fn_1
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill:
// CHECK-DAG: (b, c),
// CHECK-DAG: (c, b),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void fn_2(void) {
  int e1, e2, a, b, c, q, n;
  if (e1 < e2)
    b += a<e1;

  if (a)
    q = a;

  if (b < c)
    f((b = 2) + 1);
  else
    q = n;

// CHECK-LABEL: fn_2
// CHECK-NEXT: Block #9: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In: (e1, e2),
// CHECK-NEXT: Kill:
// CHECK-DAG: (b, c),
// CHECK-DAG: (c, b),
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (c, b),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill:
// CHECK-DAG: (b, c),
// CHECK-DAG: (c, b),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void fn_3(void) {
  int a, b, c;
  if (a ? b : (c>=2))
    a = b;

// CHECK-LABEL: fn_3
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void fn_4(void) {
  int a, b, c;
  while ((c = a) < 2) {
    if (c < b)
      c = c + b;
  }

// CHECK-LABEL: fn_4
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (c, b),
// CHECK-DAG: (b, c),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In: (c, b),
// CHECK-NEXT: Kill:
// CHECK-DAG: (c, b),
// CHECK-DAG: (b, c),
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

_Nt_array_ptr<int> g(int a) : byte_count(a);
_Nt_array_ptr<int> fn_5(int a) {
  int d;
  if (d > a)
    return 0;

  _Nt_array_ptr<int> p : byte_count(d) = g(a);
  return 0;

// CHECK-LABEL: fn_5
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In: (d, a),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (a, d),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

_Nt_array_ptr<int> fn_6(int a) {
  int d;
  if (d <= a) {
    _Nt_array_ptr<int> p : byte_count(d) = g(a);
    return 0;
  }
  return 0;

// CHECK-LABEL: fn_6
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In: (a, d),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (d, a),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void fn_7(void) {
  int a, b, c;
  while (a++ < 2) {
  L:if (a < b)
      c++;
    if (c < b)
      goto L;
  }

// CHECK-LABEL: fn_7
// CHECK-NEXT: Block #9: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (a, b),
// CHECK-DAG: (b, a),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In: (a, b),
// CHECK-NEXT: Kill:
// CHECK-DAG: (c, b),
// CHECK-DAG: (b, c),
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In: (c, b),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

// --- Facts should not contain volatiles or calls --- //

void fn_8(void) {
  volatile int a;
  int c;
  if (a < c)
    a = c;
  if (f(c) < c)
    c = a;

// CHECK-LABEL: fn_8
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

// --- Handling logical AND and logical OR --- //

void fn_9(void) {
  int a, b, c, d;
  if (a < b && b < c) {
    if (a != 1 || b != 2 || c != 3)
      d = 0;
    else
      d = 1;
  } else
    c = 2;
  c = 3;

// CHECK-LABEL: fn_9
// CHECK-NEXT: Block #10: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #9: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (b, c),
// CHECK-DAG: (3, c),
// CHECK-DAG: (c, 3),
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-DAG: (a, b),
// CHECK-DAG: (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-DAG: (a, b),
// CHECK-DAG: (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-DAG: (a, b),
// CHECK-DAG: (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-DAG: (a, b),
// CHECK-DAG: (b, c),
// CHECK-DAG: (1, a),
// CHECK-DAG: (a, 1),
// CHECK-DAG: (2, b),
// CHECK-DAG: (b, 2),
// CHECK-DAG: (3, c),
// CHECK-DAG: (c, 3),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-DAG: (a, b),
// CHECK-DAG: (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (b, c),
// CHECK-DAG: (3, c),
// CHECK-DAG: (c, 3),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

// -- Handling pointer derefs --- //

int h(int *p);
void fn_10(void) {
  int *p, *q;
  int a, b, c;

  if (*p < a)
    b = 1;
  q = &a;

  if (*p < q[1])
    b=1;
  *q = 5;

// CHECK-LABEL: fn_10
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In: (*p, a),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (*p, q[1]),
// CHECK-DAG: (q[1], *p),
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (*p, a),
// CHECK-DAG: (a, *p),
// CHECK-DAG: (*p, q[1]),
// CHECK-DAG: (q[1], *p),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void o(int *p);
void fn_11(void) {
  int a, *p, b;
  if (*p < a) {
    b = 2;
    o(p);
  }

// CHECK-LABEL: fn_11
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (*p, a),
// CHECK-NEXT: Kill:
// CHECK-DAG: (*p, a),
// CHECK-DAG: (a, *p),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

struct st { int x; int y; };
void fn_12(void) {
  struct st *st_a;
  int b, a, *q;

  if (st_a->x < a)
    b = 1;
  if ((*st_a).x < b)
    a = 1;
  else
    if (*(q + 4) <= 8)
      a = 3;
  (*st_a).x = 7;

// CHECK-LABEL: fn_12
// CHECK-NEXT: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In: (st_a->x, a),
// CHECK-NEXT: Kill:
// CHECK-DAG: ((*st_a).x, b),
// CHECK-DAG: (b, (*st_a).x),
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In: (b, (*st_a).x),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-DAG: (b, (*st_a).x),
// CHECK-DAG: (*(q + 4), 8),
// CHECK-NEXT: Kill:
// CHECK-DAG: (st_a->x, a),
// CHECK-DAG: (a, st_a->x),
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In: ((*st_a).x, b),
// CHECK-NEXT: Kill:
// CHECK-DAG: (st_a->x, a),
// CHECK-DAG: (a, st_a->x),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (st_a->x, a),
// CHECK-DAG: (a, st_a->x),
// CHECK-DAG: ((*st_a).x, b),
// CHECK-DAG: (b, (*st_a).x),
// CHECK-DAG: (8, *(q + 4)),
// CHECK-DAG: (*(q + 4), 8),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

// --- More complex cases --- //

void fn_13(void) {
  static int x, y;
  int i, a;
  y = a = x;
  if (x != 0) return;
  for (i = 0, x = (x + i) & 1023; i < a; i++)
    _Unchecked { printf("\n"); }

// CHECK-LABEL: fn_13
// CHECK-NEXT: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-DAG: (0, x),
// CHECK-DAG: (x, 0),
// CHECK-NEXT: Kill:
// CHECK-DAG: (0, x),
// CHECK-DAG: (x, 0),
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void fn_14(void) {
  int a, b, c, d;
  _Array_ptr<int> arr1 : count(10) = 0;
  _Array_ptr<int> arr2 : count(10) = 0;
  _Array_ptr<int> arr3 : count(10) = 0;

  d = 0;
  for (c = 1; c <= b; c++) {
    if ((arr1[c] <= a) && (arr2[c] >= a)) {
      arr2[c] = 1;
      d++;
    }
    else {
      arr2[c] = 0;
    }
  }
  while (d > 0) {
    arr3[d] = 0;
    d--;
  }

// CHECK-LABEL: fn_14
// CHECK-NEXT: Block #12: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #11: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (arr1[c], a),
// CHECK-DAG: (a, arr2[c]),
// CHECK-NEXT: }
// CHECK-NEXT: Block #10: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (arr1[c], a),
// CHECK-DAG: (a, arr2[c]),
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #9: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (arr1[c], a),
// CHECK-DAG: (a, arr2[c]),
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-DAG: (arr1[c], a),
// CHECK-DAG: (a, arr2[c]),
// CHECK-NEXT: Kill:
// CHECK-DAG: (arr1[c], a),
// CHECK-DAG: (a, arr2[c]),
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (arr1[c], a),
// CHECK-DAG: (a, arr2[c]),
// CHECK-NEXT: }
}

int j(_Ptr<char> i);
_Ptr<char> k(void);
void fn_15(void) {
  unsigned int i = 0;
  int m = 0;
  _Ptr<char> r = 0;
  switch (j(r)) {
    case 0:
      r = k();
      if (r == 0) {
        m = 9;
        break;
      }
      m = j(r);
      for (i = 0; i < j(r); i++) {
        if (m == 0) return;
        m++;
      }
      break;
    case 1:
    case 3:
      if (m <= 8)
        m--;
      break;
    case 13:
      break;
    default:
      return;
  }
  m = -1;

// CHECK-LABEL: fn_15
// CHECK-NEXT: Block #18: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #17: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (r, 0),
// CHECK-DAG: (0, r),
// CHECK-NEXT: }
// CHECK-NEXT: Block #15: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (0, m),
// CHECK-DAG: (m, 0),
// CHECK-DAG: (8, m),
// CHECK-DAG: (m, 8),
// CHECK-NEXT: }
// CHECK-NEXT: Block #14: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #9: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #13: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #11: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (0, m),
// CHECK-DAG: (m, 0),
// CHECK-DAG: (8, m),
// CHECK-DAG: (m, 8),
// CHECK-NEXT: }
// CHECK-NEXT: Block #10: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #12: {
// CHECK-NEXT: In:
// CHECK-DAG: (0, m),
// CHECK-DAG: (m, 0),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #16: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (0, m),
// CHECK-DAG: (m, 0),
// CHECK-DAG: (8, m),
// CHECK-DAG: (m, 8),
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In: (m, 8),
// CHECK-NEXT: Kill:
// CHECK-DAG: (0, m),
// CHECK-DAG: (m, 0),
// CHECK-DAG: (8, m),
// CHECK-DAG: (m, 8),
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (0, m),
// CHECK-DAG: (m, 0),
// CHECK-DAG: (8, m),
// CHECK-DAG: (m, 8),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

struct st_80;
struct st_80_arr {
    struct st_80 **e : itype(_Array_ptr<_Ptr<struct st_80>>) count(c);
    int d;
    int c;
};
void fn_16(_Ptr<struct st_80_arr> arr, int b) {
  _Array_ptr<_Ptr<struct st_80>> a : count(b) = 0;
  if (arr->c <= b) {
    arr->c = b * b, arr->e = a;
  }
// CHECK-LABEL: fn_16
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (arr->c, b),
// CHECK-NEXT: Kill:
// CHECK-DAG: (arr->c, b),
// CHECK-DAG: (b, arr->c),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void fn_17(void) {
  int a, b, c;
  while (((c) = a) < 2) {
    if (c < b)
      ((c))++;
  }
// CHECK-LABEL: fn_17
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (c, b),
// CHECK-DAG: (b, c),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In: (c, b),
// CHECK-NEXT: Kill:
// CHECK-DAG: (c, b),
// CHECK-DAG: (b, c),
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void fn_18(void) {
  int a, b;
  _Array_ptr<int> v1 : count(3) = 0;
  int v2 _Checked[2];
  int v3 _Checked[3][3];
  if (v2[1] < v2[0])
    (v1[0])++;
  v3[0][a]--;

// CHECK-LABEL: fn_18
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (v2[1], v2[0]),
// CHECK-NEXT: Kill:
// CHECK-DAG: (v2[1], v2[0]),
// CHECK-DAG: (v2[0], v2[1]),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (v2[1], v2[0]),
// CHECK-DAG: (v2[0], v2[1]),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void fn_19(void) {
  int a, b;
  _Array_ptr<int> v1 : count(3) = 0;
  int v2 _Checked[2];
  if (v2[1] < v2[0])
    (v1[0])++;
  (*v2)++;

// CHECK-LABEL: fn_19
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (v2[1], v2[0]),
// CHECK-NEXT: Kill:
// CHECK-DAG: (v2[1], v2[0]),
// CHECK-DAG: (v2[0], v2[1]),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (v2[1], v2[0]),
// CHECK-DAG: (v2[0], v2[1]),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

typedef struct {
  int f1;
} S1;
void fn_20(void) {
  int a;
  _Array_ptr<S1> gp1 : count(3) = 0;
  _Array_ptr<S1> gp3 : count(3) = 0;
  if (gp3->f1 != 0)
    ++(a);
  (gp3 + 2)->f1 += 1;

// CHECK-LABEL: fn_20
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-DAG: (0, gp3->f1),
// CHECK-DAG: (gp3->f1, 0),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}
