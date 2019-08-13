// Tests for dumping of datafow analysis for collecting facts
//
// RUN: %clang_cc1 -fdump-extracted-comparison-facts %s 2>1 | FileCheck %s

//===================================================================
// Dumps of different kinds of collected dataflow facts
//===================================================================

int f(int a);
void fn_1(void) {
  int a, b, c;
  if (b < c)
    if (f((a=5)+3))
      b = c;
}

// CHECK: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #2: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #1: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill: (b, c), (c, b),
// CHECK-NEXT: }
// CHECK: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }

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
}

// CHECK: Block #8: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #7: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #6: {
// CHECK-NEXT: In: (e1, e2),
// CHECK-NEXT: Kill: (b, c), (c, b),
// CHECK-NEXT: }
// CHECK: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #1: {
// CHECK-NEXT: In: (c, b),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #2: {
// CHECK-NEXT: In: (b, c),
// CHECK-NEXT: Kill: (b, c), (c, b),
// CHECK-NEXT: }
// CHECK: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }

void fn_3(void) {
  int a, b, c;
  if (a ? b : (c>=2))
    a = b;
}

// CHECK: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }


void fn_4(void) {
  int a, b, c;
  while ((c = a) < 2) {
    f(a = c);
    c = c + b;
  }
}

// CHECK: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #2: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }

_Nt_array_ptr<int> g(int a) : byte_count(a);
_Nt_array_ptr<int> fn_5(int a) {
  int d;
  if (d > a)
  return 0;

  _Nt_array_ptr<int> p : byte_count(d) = g(a);
  return p;
}

// CHECK: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #1: {
// CHECK-NEXT: In: (d, a),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #2: {
// CHECK-NEXT: In: (a, d),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }

_Nt_array_ptr<int> fn_6(int a) {
  int d;
  if (d <= a) {
    _Nt_array_ptr<int> p : byte_count(d) = g(a);
    return p;
  }
  return 0;
}

// CHECK: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #1: {
// CHECK-NEXT: In: (a, d),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #2: {
// CHECK-NEXT: In: (d, a),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }

void fn_7(void) {
  int a, b, c;
  while (a++ < 2) {
    if (a < b)
      c++;
  }
}

// CHECK: Block #6: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #5: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #4: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill: (a, b), (b, a),
// CHECK-NEXT: }
// CHECK: Block #0: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #3: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #2: {
// CHECK-NEXT: In: (a, b),
// CHECK-NEXT: Kill:
// CHECK-NEXT: }
// CHECK: Block #1: {
// CHECK-NEXT: In:
// CHECK-NEXT: Kill:
// CHECK-NEXT: }

void fn_8(void) {
  volatile int a;
  int b, n, c;
  if (a < b)
    n = c;
}

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