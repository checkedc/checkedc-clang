// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/add_bounds.c -- | diff %t.checked/add_bounds.c -

void foo(_Array_ptr<int> b, int n) { }
// CHECK_ALL: void foo(_Array_ptr<int> b : count(n), int n) _Checked { }

void bar(_Array_ptr<int> b, int n, int *c) { }
// CHECK_ALL: void bar(_Array_ptr<int> b : count(n), int n, _Ptr<int> c) _Checked { }

_Array_ptr<int> baz(_Array_ptr<int> b) {
// CHECK_ALL: _Array_ptr<int> baz(_Array_ptr<int> b : count(10)) : count(10) _Checked {
  foo(b, 10);
  return b;
}

void buz(int *);
void fiz(int * a : itype(_Array_ptr<int>) count(n), int n);
void fuz(int * a : itype(_Array_ptr<int>)) {
//CHECK_ALL: void fuz(int * a : itype(_Array_ptr<int>) count(4)) {
  fiz(a, 4);
  buz(a);
}

void biz(){
  _Array_ptr<int> x : count(1) = 0;
  _Array_ptr<int> y  = x;
  //CHECK_ALL: _Array_ptr<int> y : count(1) = x;

  int a[10];
  _Array_ptr<int> b = a;
  //CHECK_ALL:  int a _Checked[10];
  //CHECK_ALL: _Array_ptr<int> b : count(10) = a;
}

#include <stdlib.h>
_Array_ptr<int> faz(void) {
//CHECK_ALL: _Array_ptr<int> faz(void) : count(100) {
  int *c = malloc(100 * sizeof(int));
  //CHECK_ALL: _Array_ptr<int> c : count(100) = malloc<int>(100 * sizeof(int));

  return c;
}
