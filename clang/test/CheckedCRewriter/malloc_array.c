// RUN: cconv-standalone %s -- | FileCheck -match-full-lines --check-prefixes="CHECK_NOALL" %s
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stddef.h>
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

#include <stddef.h>
_Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
_Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
_Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

int * foo(int *x) {
//CHECK_NOALL: int * foo(int *x) {
//CHECK_ALL: _Array_ptr<int> foo(int *x : itype(_Array_ptr<int>)) {
  x[2] = 1;
  return x;
}
void bar(void) {
  int *y = malloc(sizeof(int)*2);
  //CHECK_NOALL: int *y = malloc<int>(sizeof(int)*2);
  //CHECK_ALL: int *y = malloc<int>(sizeof(int)*2);
  y = (int *)5;
  int *z = foo(y);
  //CHECK_NOALL: int *z = foo(y);
  //CHECK_ALL: _Ptr<int> z =  foo(y);
}
