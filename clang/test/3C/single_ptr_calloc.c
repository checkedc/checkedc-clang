// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/single_ptr_calloc.c -- | diff %t.checked/single_ptr_calloc.c -

#include <stdlib.h>

void foo(int *w) {
  //CHECK: void foo(_Ptr<int> w) {
  /*only allocating 1 thing, so should be converted even without alltypes*/
  int *x = calloc(1, sizeof(int));
  //CHECK: _Ptr<int> x = calloc<int>(1, sizeof(int));
  *x = 5;

  /*allocating multiple things, should only be converted when alltypes is on*/
  int *y = calloc(5, sizeof(int));
  //CHECK_NOALL: int *y = calloc<int>(5, sizeof(int));
  //CHECK_ALL: _Ptr<int> y = calloc<int>(5, sizeof(int));
}
