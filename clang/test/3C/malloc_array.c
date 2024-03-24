// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/malloc_array.c -- | diff %t.checked/malloc_array.c -

#include <stdlib.h>

int *foo(int *x) {
  //CHECK_NOALL: int *foo(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  //CHECK_ALL: _Array_ptr<int> foo(_Array_ptr<int> x : count(2 + 1)) : count(2 + 1) _Checked {
  x[2] = 1;
  return x;
}
void bar(void) {
  int *y = malloc(sizeof(int) * 2);
  //CHECK: int *y = malloc<int>(sizeof(int) * 2);
  y = (int *)5;
  //CHECK: y = (int *)5;
  int *z = foo(y);
  //CHECK_NOALL: _Ptr<int> z = foo(y);
  //CHECK_ALL: _Ptr<int> z = foo(_Assume_bounds_cast<_Array_ptr<int>>(y, bounds(unknown)));
}

void force(int *x) {}
//CHECK: void force(_Ptr<int> x) _Checked {}
