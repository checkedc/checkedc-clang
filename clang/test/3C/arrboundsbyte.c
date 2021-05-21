// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o %t1.unused -

/*
Array bounds (byte_bound) tests with data-flow analysis.
*/

#include <stdlib.h>
// This test needs a custom version of memcpy where src and dest are `int *`.
int *memcpy_int(int *restrict dest
                : itype(restrict _Array_ptr<int>) byte_count(n),
                  const int *restrict src
                : itype(restrict _Array_ptr<const int>) byte_count(n), size_t n)
    : itype(_Array_ptr<int>) byte_count(n);

struct foo {
  int *y;
  int l;
};
//CHECK: _Array_ptr<int> y : byte_count(l);
void bar(int *x, int c) {
  struct foo f = {x, c};
  int *q = malloc(sizeof(int) * c);
  memcpy_int(q, f.y, c);
}
//CHECK: void bar(_Array_ptr<int> x : byte_count(c), int c) {
//CHECK: _Array_ptr<int> q : count(c) = malloc<int>(sizeof(int) * c);
