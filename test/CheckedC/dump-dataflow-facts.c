// Tests for dumping of datafow analysis for collecting facts
//
// RUN: %clang_cc1 -fcheckedc-extension -fdump_extracted_comparison_facts %s 2>1 | FileCheck %s

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
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #3: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #2: {
// CHECK: In: (b, c),
// CHECK: Kill:
// CHECK: }
// CHECK: Block #1: {
// CHECK: In: (b, c),
// CHECK: Kill: (b, c), (c, b),
// CHECK: }
// CHECK: Block #0: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }

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
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #7: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #6: {
// CHECK: In: (e1, e2),
// CHECK: Kill: (b, c), (c, b),
// CHECK: }
// CHECK: Block #5: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #4: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #3: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #1: {
// CHECK: In: (c, b),
// CHECK: Kill:
// CHECK: }
// CHECK: Block #2: {
// CHECK: In: (b, c),
// CHECK: Kill: (b, c), (c, b),
// CHECK: }
// CHECK: Block #0: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }

void fn_3(void) {
  int a, b, c;
  if (a ? b : (c>=2))
    a = b;
}

// CHECK: Block #6: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #5: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #4: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #3: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #2: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #1: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #0: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }


void fn_4(void) {
  int a, b, c;
  while ((c = a) < 2) {
    f(a = c);
    c = c + b;
  }
}

// CHECK: Block #5: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #4: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #3: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #0: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #2: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #1: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }

_Nt_array_ptr<int> g(int a) : byte_count(a);
_Nt_array_ptr<int> fn_5(int a) {
  int d;
  if (d > a)
  return 0;

  _Nt_array_ptr<int> p : byte_count(d) = g(a);
  return p;
}

// CHECK: Block #4: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #3: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #1: {
// CHECK: In: (d, a),
// CHECK: Kill:
// CHECK: }
// CHECK: Block #2: {
// CHECK: In: (a, d),
// CHECK: Kill:
// CHECK: }
// CHECK: Block #0: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }

_Nt_array_ptr<int> fn_6(int a) {
  int d;
  if (d <= a) {
    _Nt_array_ptr<int> p : byte_count(d) = g(a);
    return p;
  }
  return 0;
}

// CHECK: Block #4: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #3: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #1: {
// CHECK: In: (a, d),
// CHECK: Kill:
// CHECK: }
// CHECK: Block #2: {
// CHECK: In: (d, a),
// CHECK: Kill:
// CHECK: }
// CHECK: Block #0: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }

void fn_7(void) {
  int a, b, c;
  while (a++ < 2) {
    if (a < b)
      c++;
  }
}

// CHECK: Block #6: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #5: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #4: {
// CHECK: In:
// CHECK: Kill: (a, b), (b, a),
// CHECK: }
// CHECK: Block #0: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #3: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }
// CHECK: Block #2: {
// CHECK: In: (a, b),
// CHECK: Kill:
// CHECK: }
// CHECK: Block #1: {
// CHECK: In:
// CHECK: Kill:
// CHECK: }