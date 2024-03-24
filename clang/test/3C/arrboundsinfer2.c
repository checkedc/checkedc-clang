// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o %t1.unused -

/*
Basic array bounds tests (without any data-flow analysis).
*/

#include <stdlib.h>

extern void ext_func(_Array_ptr<int> arr : count(len), unsigned len);

struct bar {
  char *a;
  unsigned h;
  unsigned b;
};

//CHECK: _Array_ptr<char> a : count(b);

int foo(int *arr, unsigned len) {
  unsigned i = 0;
  int *arr1 = malloc(sizeof(int) * len);
  arr1[0] = 0;
  int *arr2 = malloc(sizeof(int) * len);
  arr2[1] = 0;
  int *arr3;
  ext_func(arr3, len);
  for (i = 0; i < len; i++) {
    arr[i] = 0;
  }
  return 0;
}

//CHECK: int foo(_Array_ptr<int> arr : count(len), unsigned len) {
//CHECK: _Array_ptr<int> arr1 : count(len) = malloc<int>(sizeof(int) * len);
//CHECK: _Array_ptr<int> arr2 : count(len) = malloc<int>(sizeof(int) * len);
//CHECK: _Array_ptr<int> arr3 : count(len) = ((void *)0);

void baz() {
  unsigned n;
  struct bar c;
  int *arr1;
  foo(arr1, n);
  c.a = malloc(c.b * sizeof(char));
  c.a[0] = 0;
}

//CHECK: _Array_ptr<int> arr1 : count(n) = ((void *)0);
