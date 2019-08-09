// Tests for dumping of datafow analysis for collecting facts
//
// RUN: %clang_cc1 -DDEBUG_DATAFLOW -fcheckedc-extension %s 2>&1 | FileCheck %s

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

// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: OutThen: (b, c),
// CHECK: OutElse: (c, b),
// CHECK: }
// CHECK: {
// CHECK: In: (b, c),
// CHECK: OutThen: (b, c),
// CHECK: OutElse: (b, c),
// CHECK: }
// CHECK: {
// CHECK: In: (b, c),
// CHECK: }
// CHECK: {
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

// Check: {
// Check: }
// Check: {
// Check: OutThen: (e1, e2),
// Check: OutElse: (e2, e1),
// Check: }
// Check: {
// Check: In: (e1, e2),
// Check: OutThen: (e1, e2),
// Check: OutElse: (e1, e2),
// Check: }
// Check: {
// Check: }
// Check: {
// Check: }
// Check: {
// Check: OutThen: (b, c),
// Check: OutElse: (c, b),
// Check: }
// Check: {
// Check: In: (c, b),
// Check: OutThen: (c, b),
// Check: OutElse: (c, b),
// Check: }
// Check: {
// Check: In: (b, c),
// Check: }
// Check: {
// Check: }

void fn_3(void) {
  int a, b, c;
  if (a ? b : (c>=2))
  	a = b;
}

// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }

void fn_4(void) {
  int a, b, c;
  while (c = a < 2) {
  	f(a = c);
  	c = c + b;
  }
}

// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }

_Nt_array_ptr<int> g(int a) : byte_count(a);
_Nt_array_ptr<int> fn_5(int a) {
  int d;
  if (d > a)
	return 0;

  _Nt_array_ptr<int> p : byte_count(d) = g(a); // expected-warning {{cannot prove declared bounds for 'p' are valid after initialization}} \
                                               // expected-note {{(expanded) declared bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + d)'}} \
                                               // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of g(a), (_Array_ptr<char>)value of g(a) + a)'}}
  return p;
}

// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: OutThen: (a, d),
// CHECK: OutElse: (d, a),
// CHECK: }
// CHECK: {
// CHECK: In: (d, a),
// CHECK: OutThen: (d, a),
// CHECK: OutElse: (d, a),
// CHECK: }
// CHECK: {
// CHECK: In: (a, d),
// CHECK: OutThen: (a, d),
// CHECK: OutElse: (a, d),
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }

_Nt_array_ptr<int> fn_6(int a) {
  int d;
  if (d <= a) {
    _Nt_array_ptr<int> p : byte_count(d) = g(a); // expected-warning {{cannot prove declared bounds for 'p' are valid after initialization}} \
                                               // expected-note {{(expanded) declared bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + d)'}} \
                                               // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of g(a), (_Array_ptr<char>)value of g(a) + a)'}}
    return p;
  }
  return 0;
}

// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: OutThen: (d, a),
// CHECK: OutElse: (a, d),
// CHECK: }
// CHECK: {
// CHECK: In: (a, d),
// CHECK: OutThen: (a, d),
// CHECK: OutElse: (a, d),
// CHECK: }
// CHECK: {
// CHECK: In: (d, a),
// CHECK: OutThen: (d, a),
// CHECK: OutElse: (d, a),
// CHECK: }
// CHECK: {
// CHECK: }

void fn_7(void) {
  int a, b, c;
  while (a++ < 2) {
    if (a < b)
      c++;
  }
}

// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: }
// CHECK: {
// CHECK: OutThen: (a, b),
// CHECK: OutElse: (b, a),
// CHECK: }
// CHECK: {
// CHECK: In: (a, b),
// CHECK: OutThen: (a, b),
// CHECK: OutElse: (a, b),
// CHECK: }
// CHECK: {
// CHECK: }