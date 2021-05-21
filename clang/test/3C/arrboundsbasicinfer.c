// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o %t1.unused -

/*
Basic array bounds tests (without any data-flow analysis).
*/

#include <stdlib.h>

struct bar {
  char *a;
  unsigned h;
  unsigned b;
};

//CHECK: _Array_ptr<char> a : count(b);

int foo(int *arr, unsigned len) {
  unsigned i = 0;
  for (i = 0; i < len; i++) {
    arr[i] = 0;
  }
  return 0;
}

//CHECK: int foo(_Array_ptr<int> arr : count(len), unsigned len) {

void baz() {
  unsigned n;
  struct bar c;
  int *arr1;
  foo(arr1, n);
  c.a = malloc(c.b * sizeof(char));
  c.a[0] = 0;
}

//CHECK: _Array_ptr<int> arr1 : count(n) = ((void *)0);
