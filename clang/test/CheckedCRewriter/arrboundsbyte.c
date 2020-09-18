// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s


/*
Array bounds (byte_bound) tests with data-flow analysis.
*/

#include <stddef.h>
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
int *memcpy(int * restrict dest : itype(restrict _Array_ptr<int>) byte_count(n),
             const int * restrict src : itype(restrict _Array_ptr<const int>) byte_count(n),
             size_t n) : itype(_Array_ptr<int>) byte_count(n);
struct foo {
  int *y;
  int l;
};
//CHECK: _Array_ptr<int> y : byte_count(l);
void bar(int *x, int c) {
  struct foo f = { x, c };
  int *q = malloc(sizeof(int)*c);
  memcpy(q,f.y,c);
}
//CHECK: void bar(_Array_ptr<int> x : byte_count(c), int c) {
//CHECK: _Array_ptr<int> q =  malloc<int>(sizeof(int)*c);
