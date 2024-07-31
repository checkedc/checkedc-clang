// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/multivardecls.c -- -Wno-error=int-conversion | diff %t.checked/multivardecls.c -

#include <stddef.h>
#include <stdlib.h>

void test() {
  int *a = (int *)0, *b = (int *)0;
  //CHECK: _Ptr<int> a = (_Ptr<int>)0;
  //CHECK: _Ptr<int> b = (_Ptr<int>)0;

  int *c = (int *)1, *d = (int *){0};
  //CHECK: int *c = (int *)1;
  //CHECK: _Ptr<int> d = (_Ptr<int>){0};

  int *e, *f = malloc(sizeof(int));
  //CHECK: _Ptr<int> e = ((void *)0);
  //CHECK: _Ptr<int> f = malloc<int>(sizeof(int));

  int g[1] = {*&(int){1}}, *h;
  //CHECK_ALL: int g _Checked[1] = {*&(int){1}};
  //CHECK_NOALL: int g[1] = {*&(int){1}};
  //CHECK: _Ptr<int> h = ((void *)0);

  float *i = 1, j = 0, *k = (float *){0}, l;
  //CHECK: float *i = 1;
  //CHECK: float j = 0;
  //CHECK: _Ptr<float> k = (_Ptr<float>){0};
  //CHECK: float l;

  int m = 1, *n = &m;
  //CHECK: int m = 1, *n = &m;
  n++;

  int o, p[1], q[] = {1, 2}, r[1][1], *s;
  //CHECK: int o;
  //CHECK_ALL: int p _Checked[1];
  //CHECK_NOALL: int p[1];
  //CHECK_ALL: int q _Checked[] = {1, 2};
  //CHECK_NOALL: int q[] = {1, 2};
  //CHECK_ALL: int r _Checked[1] _Checked[1];
  //CHECK_NOALL: int r[1][1];
  //CHECK: _Ptr<int> s = ((void *)0);

  int *t[1], *u = malloc(2 * sizeof(int)), *v;
  //CHECK_ALL: _Ptr<int> t _Checked[1] = {((void *)0)};
  //CHECK_NOALL: int *t[1];
  //CHECK_ALL: _Ptr<int> u = malloc<int>(2 * sizeof(int));
  //CHECK_NOALL: int *u = malloc<int>(2 * sizeof(int));
  //CHECK: _Ptr<int> v = ((void *)0);

  int *w = (int *)0, *x = (int *)0, *z = (int *)1;
  //CHECK: _Ptr<int> w = (_Ptr<int>)0;
  //CHECK: int *x = (int *)0;
  //CHECK: int *z = (int *)1;
  x = (int *)1;
  //CHECK: x = (int *)1;
}

void test2() {
  int a, b[1][1], *c;
  // CHECK: int a;
  // CHECK_ALL: int b _Checked[1] _Checked[1];
  // CHECK_NOALL: int b[1][1];
  // CHECK: _Ptr<int> c = ((void *)0);

  int e[1][1], *f, *g[1] = {(int *)0};
  // CHECK_ALL: int e _Checked[1] _Checked[1];
  // CHECK_NOALL: int e[1][1];
  // CHECK: _Ptr<int> f = ((void *)0);
  // CHECK_ALL: _Ptr<int> g _Checked[1] = {(_Ptr<int>)0};
  // CHECK_NOALL: int *g[1] = {(int *)0};

  int h, (*i)(int), *j, (*k)(int);
  // CHECK: int h;
  // CHECK: _Ptr<int (int)> i = ((void *)0);
  // CHECK: _Ptr<int> j = ((void *)0);
  // CHECK: int (*k)(int);

  k = 1;
}

void test3() {
  int *a, b;
  // CHECK: _Ptr<int> a = ((void *)0);
  // CHECK: int b;

  int *c, d;
  // CHECK: _Ptr<int> c = ((void *)0);
  // CHECK: int d;

  int *e, f;
  // CHECK: _Ptr<int> e = ((void *)0);
  // CHECK: int f;

  int *h, g;
  // CHECK: _Ptr<int> h = ((void *)0);
  // CHECK: int g;
}

void test4() {
  struct foo {
    int *a, b;
    // CHECK: _Ptr<int> a;
    // CHECK: int b;

    int c, *d;
    // CHECK: int c;
    // CHECK: _Ptr<int> d;

    int *f, **g;
    // CHECK: _Ptr<int> f;
    // CHECK: _Ptr<_Ptr<int>> g;
  };
}

int *a;
int b;
int **c;
// CHECK: _Ptr<int> a = ((void *)0);
// CHECK: int b;
// CHECK: _Ptr<_Ptr<int>> c = ((void *)0);

int *d, e, **f;
// CHECK: _Ptr<int> d = ((void *)0);
// CHECK: int e;
// CHECK: _Ptr<_Ptr<int>> f = ((void *)0);

// Simple test that storage and type qualifiers are preserved on both global and
// function-scope variables.

static const int *sd, se, **sf;
// CHECK: static _Ptr<const int> sd = ((void *)0);
// CHECK: static const int se;
// CHECK: static _Ptr<_Ptr<const int>> sf = ((void *)0);

void test5() {
  static const int *fsd, fse, **fsf;
  // CHECK: static _Ptr<const int> fsd = ((void *)0);
  // CHECK: static const int fse;
  // CHECK: static _Ptr<_Ptr<const int>> fsf = ((void *)0);
}

void test6() {
  int *a, *b;
  int *c, *e;
  struct foo {
    int a, *b;
    int *c, *d;
  };
}
// CHECK: _Ptr<int> a = ((void *)0);
// CHECK: _Ptr<int> b = ((void *)0);
// CHECK: _Ptr<int> c = ((void *)0);
// CHECK: _Ptr<int> e = ((void *)0);
// CHECK: struct foo {
// CHECK: int a;
// CHECK: _Ptr<int> b;
// CHECK: _Ptr<int> c;
// CHECK: _Ptr<int> d;
// CHECK: };

void test7() {
  // Test that variables that require struct initialization honor base type
  // renames the same way as global variables.
  struct { int *x; } s7;
  //CHECK: struct s7_struct_1 { _Ptr<int> x; };
  //CHECK: struct s7_struct_1 s7 = {};
}

// Test that getNextComma doesn't falsely trigger on commas inside a bounds
// annotation. The scan shouldn't start until after the declaration source
// range, which should include the bounds annotation, and it's unlikely that a
// change to 3C could break that without also breaking other tests, but it
// doesn't hurt to have a specific test for commas too. The extra nested comma
// expression `(0, lo)` was needed to trigger the bug in older versions of 3C:
// the lexer didn't seem to report the comma that is part of the `bounds`
// construct to getNextComma as a comma token.
//
// `p3` is needed to trigger the multi-decl to be broken up at all.
_Array_ptr<int> lo, hi;
_Array_ptr<int> p1 : bounds((0, lo), hi), p2 : bounds(lo, (0, hi)), *p3;
//CHECK: _Array_ptr<int> p1 : bounds((0, lo), hi);
// The extra space after `0` seems to be because Decl::print treats the comma
// operator like any other binary operator such as `+` and adds spaces both
// before and after it. (TODO: Research whether this has already been discussed
// in upstream Clang and if not, file a bug there?)
//CHECK: _Array_ptr<int> p2 : bounds(lo, (0 , hi));
//CHECK: _Ptr<_Array_ptr<int>> p3 = ((void *)0);

// Simple tests of typedef multi-decls from
// https://github.com/correctcomputation/checkedc-clang/issues/651.
// inline_anon_structs.c has a few additional tests of typedef multi-decls
// involving inline structs.

typedef int *A, *B;
// CHECK: typedef _Ptr<int> A;
// CHECK: typedef _Ptr<int> B;

void foo(void) {
  A a;
  B b;
}

typedef int *C, *D;
// CHECK: typedef _Ptr<int> C;
// CHECK: typedef int *D;

void bar(void) {
  C c;
  D d = (D)1;
}
