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
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill: (b, c), (c, b),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
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
// CHECK-NEXT: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In: (e1, e2),
// CHECK-NEXT: Kill: (b, c), (c, b),
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
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In: (c, b),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill: (b, c), (c, b),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
}

void fn_3(void) {
  int a, b, c;
  if (a ? b : (c>=2))
    a = b;

// CHECK-LABEL: fn_3
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
// CHECK-NEXT: Block #0: {
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
// CHECK-NEXT: Kill: (c, b), (b, c),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: (c, b),
// CHECK-NEXT: Kill: (c, b), (b, c),
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
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
  return p;

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
    return p;
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
// CHECK-NEXT: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill: (a, b), (b, a),
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In: (a, b),
// CHECK-NEXT: Kill: (c, b), (b, c),
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
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
// CHECK-NEXT: Block #0: {
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
// CHECK-NEXT: Kill: (b, c), (3, c), (c, 3),
// CHECK-NEXT: }
// CHECK-NEXT: Block #7: {
// CHECK-NEXT: In: (a, b), (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #6: {
// CHECK-NEXT: In: (a, b), (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In: (a, b), (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In: (a, b), (b, c), (1, a), (a, 1), (2, b), (b, 2), (3, c), (c, 3),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In: (a, b), (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill: (b, c), (3, c), (c, 3),
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
  h(q);

  if (*p < q[1])
    b=1;

// CHECK-LABEL: fn_10
// CHECK-NEXT: Block #5: {
// CHECK-NEXT: In: 
// CHECK-NEXT: Kill: 
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In: 
// CHECK-NEXT: Kill: 
// CHECK-NEXT: }
// CHECK-NEXT: Block #3: {
// CHECK-NEXT: In: (*p, a), 
// CHECK-NEXT: Kill: 
// CHECK-NEXT: }
// CHECK-NEXT: Block #2: {
// CHECK-NEXT: In: 
// CHECK-NEXT: Kill: (*p, a), (a, *p), (*p, q[1]), (q[1], *p), 
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

struct st { int x; int y; };
void fn_11(void) {
  struct st *st_a;
  int b, a, *q;

  if (st_a->x < a)
    b = 1;
  if ((*st_a).x < b)
    a = 1;
  else
    if (*(q + 4) <= 8)
      a = 3;
  q = &a;

// CHECK-LABEL: fn_11
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
// CHECK-NEXT: Kill: ((*st_a).x, b), (b, (*st_a).x), 
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
// CHECK-NEXT: In: (b, (*st_a).x), (*(q + 4), 8), 
// CHECK-NEXT: Kill: (st_a->x, a), (a, st_a->x), 
// CHECK-NEXT: }
// CHECK-NEXT: Block #4: {
// CHECK-NEXT: In: ((*st_a).x, b), 
// CHECK-NEXT: Kill: (st_a->x, a), (a, st_a->x), 
// CHECK-NEXT: }
// CHECK-NEXT: Block #1: {
// CHECK-NEXT: In: 
// CHECK-NEXT: Kill: (st_a->x, a), (a, st_a->x), ((*st_a).x, b), (b, (*st_a).x), (8, *(q + 4)), (*(q + 4), 8), 
// CHECK-NEXT: }
// CHECK-NEXT: Block #0: {
// CHECK-NEXT: In: 
// CHECK-NEXT: Kill: 
// CHECK-NEXT: }
}
