// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/realloc.c -- | diff %t.checked/realloc.c -

#include <stdlib.h>

void foo(int *w) {
  //CHECK: void foo(_Ptr<int> w) {
  int *y = malloc(2 * sizeof(int));
  //CHECK_NOALL: int *y = malloc<int>(2 * sizeof(int));
  //CHECK_ALL: _Array_ptr<int> y : count(2) = malloc<int>(2 * sizeof(int));
  y[1] = 3;
  int *z = realloc(y, 5 * sizeof(int));
  //CHECK_NOALL: int *z = realloc<int>(y, 5 * sizeof(int));
  //CHECK_ALL: _Array_ptr<int> z : count(5) = realloc<int>(y, 5 * sizeof(int));
  z[3] = 2;
}
