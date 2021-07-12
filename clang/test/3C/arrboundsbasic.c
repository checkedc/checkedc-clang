// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o %t1.unused -

/*
Basic array bounds tests (without any data-flow analysis).
*/

#include <stdlib.h>

struct bar {
  char *a;
  unsigned a_len;
};
//CHECK: _Array_ptr<char> a : count(a_len);

// Here, the bounds of arr will be count(len)
int foo(int *arr, unsigned len) {
  unsigned i, n;
  struct bar a;
  char *arr1 = malloc(n * sizeof(char));
  char *arr2 = calloc(n, sizeof(char));
  arr1[0] = 0;
  arr2[0] = 0;
  for (i = 0; i < len; i++) {
    arr[i] = 0;
  }
  a.a_len = 4, a.a = malloc(a.a_len * sizeof(char));
  a.a[0] = 0;
  return 0;
}
//CHECK: int foo(_Array_ptr<int> arr : count(len), unsigned len) {
//CHECK: _Array_ptr<char> arr1 : count(n) = malloc<char>(n * sizeof(char));
//CHECK: _Array_ptr<char> arr2 : count(n) = calloc<char>(n, sizeof(char));
