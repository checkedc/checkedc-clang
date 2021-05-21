// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/calloc.c -- | diff %t.checked/calloc.c -

#include <stdlib.h>

void func(int *x : itype(_Array_ptr<int>));
//CHECK: void func(int *x : itype(_Array_ptr<int>));

void foo(int *w) {
  //CHECK: void foo(_Ptr<int> w) {
  int *x = calloc(5, sizeof(int));
  //CHECK_NOALL: int *x = calloc<int>(5, sizeof(int));
  //CHECK_ALL: _Array_ptr<int> x : count(5) = calloc<int>(5, sizeof(int));
  x[2] = 3;
  func(x);
}
