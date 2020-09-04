// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stddef.h>
_Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);


int *test() {
// CHECK_NOALL: int *test(void) {
// CHECK_ALL: _Array_ptr<int> test(void) {
  int *a = malloc(sizeof(int));
  // CHECK: _Ptr<int> a = malloc<int>(sizeof(int));
  free(a);
  // CHECK: free<int>(a);

  int *b = malloc(5*sizeof(int));
  // CHECK_NOALL: int *b = malloc<int>(5*sizeof(int));
  // CHECK_ALL: _Array_ptr<int> b : count(5) =  malloc<int>(5*sizeof(int));
  free(b);
  // CHECK: free<int>(b);
  return b;
}

_Itype_for_any(T) void my_free(void *pointer : itype(_Array_ptr<T>) byte_count(0));

int *test2() {
// CHECK_NOALL: int *test2(void) {
// CHECK_ALL:_Array_ptr<int> test2(void) {
  int *a = malloc(sizeof(int));
  // CHECK: _Ptr<int> a = malloc<int>(sizeof(int));
  my_free(a);
  // CHECK: my_free<int>(a);

  int *b = malloc(5*sizeof(int));
  // CHECK_NOALL: int *b = malloc<int>(5*sizeof(int));
  // CHECK_ALL: _Array_ptr<int> b : count(5) =  malloc<int>(5*sizeof(int));
  my_free(b);
  // CHECK: my_free<int>(b);

  return b;
}
