// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stdlib.h>

int *test() {
  // CHECK_NOALL: int *test(void) : itype(_Ptr<int>) {
  // CHECK_ALL: _Array_ptr<int> test(void) : count(5) {
  int *a = malloc(sizeof(int));
  // CHECK: _Ptr<int> a = malloc<int>(sizeof(int));
  free(a);
  // CHECK: free<int>(a);

  int *b = malloc(5 * sizeof(int));
  // CHECK_NOALL: int *b = malloc<int>(5 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> b : count(5) =  malloc<int>(5 * sizeof(int));
  free(b);
  // CHECK: free<int>(b);
  return b;
}

_Itype_for_any(T) void my_free(void *pointer
                               : itype(_Array_ptr<T>) byte_count(0));

int *test2() {
  // CHECK_NOALL: int *test2(void) : itype(_Ptr<int>) {
  // CHECK_ALL: _Array_ptr<int> test2(void) : count(5) {
  int *a = malloc(sizeof(int));
  // CHECK: _Ptr<int> a = malloc<int>(sizeof(int));
  my_free(a);
  // CHECK: my_free<int>(a);

  int *b = malloc(5 * sizeof(int));
  // CHECK_NOALL: int *b = malloc<int>(5 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> b : count(5) =  malloc<int>(5 * sizeof(int));
  my_free(b);
  // CHECK: my_free<int>(b);

  return b;
}
