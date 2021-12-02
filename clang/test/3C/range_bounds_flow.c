// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | %clang -c -Xclang -verify -Wno-unused-value -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/range_bounds_flow.c -- | diff %t.checked/range_bounds_flow.c -

// `a` is inferred as the lower bound for `b` and `c`.
void test1() {
  int *a;
  //CHECK_ALL: _Array_ptr<int> a : count(0 + 1) = ((void *)0);
  a[0];

  int *b = a;
  b++;
  //CHECK_ALL: _Array_ptr<int> b : bounds(a, a + 0 + 1) = a;

  int *c = b;
  //CHECK_ALL: _Array_ptr<int> c : bounds(a, a + 0 + 1) = b;
  c[0];
}


// Here we need to add a temporary lower bound instead.
void test2() {
  int *a;
  //CHECK_ALL: _Array_ptr<int> __3c_lower_bound_a : count(0 + 1) = ((void *)0);
  //CHECK_ALL: _Array_ptr<int> a : bounds(__3c_lower_bound_a, __3c_lower_bound_a + 0 + 1) = __3c_lower_bound_a;
  a[0];
  a++;

  int *b = a;
  //CHECK_ALL: _Array_ptr<int> b : bounds(__3c_lower_bound_a, __3c_lower_bound_a + 0 + 1) = a;

  int *c = b;
  //CHECK_ALL: _Array_ptr<int> c : bounds(__3c_lower_bound_a, __3c_lower_bound_a + 0 + 1) = b;
  c[0];
}

int *test3(int *a, int l) {
  int *b = a;
  // CHECK_ALL: _Array_ptr<int> test3(_Array_ptr<int> a : count(l), int l) : bounds(a, a + l) _Checked {
  // CHECK_ALL: _Array_ptr<int> b : bounds(a, a + l) = a;
  b++;
  return b;
}

int *test4(int *, int);
int *test4(int *x, int l);
int *test4();
// CHECK_ALL: _Array_ptr<int> test4(_Array_ptr<int> __3c_lower_bound_a : count(l), int l) : bounds(__3c_lower_bound_a, __3c_lower_bound_a + l);
// CHECK_ALL: _Array_ptr<int> test4(_Array_ptr<int> __3c_lower_bound_a : count(l), int l) : bounds(__3c_lower_bound_a, __3c_lower_bound_a + l);
// CHECK_ALL: _Array_ptr<int> test4(_Array_ptr<int> __3c_lower_bound_a : count(l), int l) : bounds(__3c_lower_bound_a, __3c_lower_bound_a + l);

int *test4(int *a, int l) {
  // CHECK_ALL: _Array_ptr<int> test4(_Array_ptr<int> __3c_lower_bound_a : count(l), int l) : bounds(__3c_lower_bound_a, __3c_lower_bound_a + l) _Checked {
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_lower_bound_a, __3c_lower_bound_a + l) = __3c_lower_bound_a;
  a++;
  return a;
}

// There are multiple possible lower bounds for `c`, but they are consistent
// with each other.
void test5(int *a, int l) {
  int *b = a;
  int *c = b;
  // CHECK_ALL: void test5(_Array_ptr<int> a : count(l), int l) _Checked {
  // CHECK_ALL: _Array_ptr<int> b : count(l) = a;
  // CHECK_ALL: _Array_ptr<int> c : bounds(b, b + l) = b;
  c++;
}

// Lower bounds aren't consistent. We can't use `a` or `b`, so a fresh lower
// bound is created  g.
void test6() {
  int *a;
  int *b;
  // CHECK_ALL: _Array_ptr<int> a : count(0 + 1) = ((void *)0);
  // CHECK_ALL: _Array_ptr<int> b : count(0 + 1) = ((void *)0);

  int *c;
  c = a;
  c = b;
  // CHECK_ALL: _Array_ptr<int> __3c_lower_bound_c : count(0 + 1) = ((void *)0);
  // CHECK_ALL: _Array_ptr<int> c : bounds(__3c_lower_bound_c, __3c_lower_bound_c + 0 + 1) = __3c_lower_bound_c;
  // CHECK_ALL: __3c_lower_bound_c = a, c = __3c_lower_bound_c;
  // CHECK_ALL: __3c_lower_bound_c = b, c = __3c_lower_bound_c;

  c++;
  c[0];
}

// Lower bound is inferred from pointer with an declared count bound.
void test7(int *a : count(l), int dummy, int l) {
  int *b = a;
  // CHECK_ALL: _Array_ptr<int> b : bounds(a, a + l) = a;
  b++;
}

// There is no valid lower bound available, but the lower bound for `a` can
// be the same as the lower bound for `b`. A fresh lower bound is created for
// `b`, and then used for `a` as well.
void test8(int *a, int *b) {
// CHECK_ALL: void test8(_Array_ptr<int> a : bounds(__3c_lower_bound_b, __3c_lower_bound_b + 0 + 1), _Array_ptr<int> __3c_lower_bound_b : count(0 + 1)) _Checked {
// CHECK_ALL: _Array_ptr<int> b : bounds(__3c_lower_bound_b, __3c_lower_bound_b + 0 + 1) = __3c_lower_bound_b;

  a++;
  b++;
  a = b;
  a[0];
}

// A cycle is formed by `a`,`b` and `c`. The lower bound `x` starts at `a`,
// propagates through `b` and `c`, and then flows into `a` again. This is
// consistent, so `x` is used as the lower bound.
void test9(int *x, int l) {
// CHECK_ALL: void test9(_Array_ptr<int> x : count(l), int l) _Checked {
  int *a = x, *b, *c;
// CHECK_ALL: _Array_ptr<int> a : bounds(x, x + l) = x;
// CHECK_ALL: _Array_ptr<int> b : bounds(x, x + l) = ((void *)0);
// CHECK_ALL: _Array_ptr<int> c : bounds(x, x + l) = ((void *)0);
  a++;
  b = a;
  c = b;
  a = c;
}

// Same as above, but now fresh lower bound needs to be created for `x`.
void test10(int *x, int l) {
// CHECK_ALL: void test10(_Array_ptr<int> __3c_lower_bound_x : count(l), int l) _Checked {
// CHECK_ALL: _Array_ptr<int> x : bounds(__3c_lower_bound_x, __3c_lower_bound_x + l) = __3c_lower_bound_x;
  x++;
  int *a = x, *b, *c;
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_lower_bound_x, __3c_lower_bound_x + l) = x;
  // CHECK_ALL: _Array_ptr<int> b : bounds(__3c_lower_bound_x, __3c_lower_bound_x + l) = ((void *)0);
  // CHECK_ALL: _Array_ptr<int> c : bounds(__3c_lower_bound_x, __3c_lower_bound_x + l) = ((void *)0);
  a++;
  b = a;
  c = b;
  a = c;
}

// Context sensitive edges should not cause `c` to be a lower bound for `b`.
void testx(int *a){ a[0]; }
void otherxx(){
  int *b;
  int *c;
  //CHECK_ALL: _Array_ptr<int> __3c_lower_bound_b : count(0 + 1) = ((void *)0);
  //CHECK_ALL: _Array_ptr<int> b : bounds(__3c_lower_bound_b, __3c_lower_bound_b + 0 + 1) = __3c_lower_bound_b;
  //CHECK_ALL: _Array_ptr<int> c : count(0 + 1) = ((void *)0);

  testx(b);
  testx(c);
  b[0];
  c[0];
  b++;
}

struct structy { int *b; };
// CHECK_ALL: struct structy { _Array_ptr<int> b; };
void testy(struct structy d) {
  // expected-error@+2 {{inferred bounds for '__3c_lower_bound_e' are unknown after initialization}}
  // expected-note@+1 {{}}
  int *e = d.b;
  // CHECK_ALL: _Array_ptr<int> __3c_lower_bound_e : count(0 + 1) = d.b;
  // CHECK_ALL: _Array_ptr<int> e : bounds(__3c_lower_bound_e, __3c_lower_bound_e + 0 + 1) = __3c_lower_bound_e;

  d.b = e;
  e++;

  e[0];
}

void foo(int *x, unsigned long s) {
// CHECK_ALL: void foo(_Array_ptr<int> x : count(s), unsigned long s) _Checked {
  for (int i = 0; i < s; i++)
    x[i];
}

void foo_caller(unsigned long l) {
  int *a;
  a++;
  // CHECK_ALL:_Array_ptr<int> __3c_lower_bound_a : count(l) = ((void *)0);
  // CHECK_ALL:_Array_ptr<int> a : bounds(__3c_lower_bound_a, __3c_lower_bound_a + l) = __3c_lower_bound_a;

  foo(a, l);
  // expected-warning@-1 {{cannot prove argument meets declared bounds for 1st parameter}}
  // expected-note@-2 {{}}
  // expected-note@-3 {{}}
}


// Lower bound inference for `b` fails because `a` is out of scope. If `a` were
// in scope, it would be used as a lower bound.
void bar(int *b) {
// CHECK_ALL: void bar(_Array_ptr<int> __3c_lower_bound_b : count(0 + 1)) _Checked {
// CHECK_ALL: _Array_ptr<int> b : bounds(__3c_lower_bound_b, __3c_lower_bound_b + 0 + 1) = __3c_lower_bound_b;
  int *a;
  // CHECK_ALL: _Array_ptr<int> a : count(0 + 1) = ((void *)0);
  b = a;
  // CHECK_ALL: __3c_lower_bound_b = a, b = __3c_lower_bound_b;
  b++;
  b[0];
}
