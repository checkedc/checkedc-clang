// RUN: cconv-standalone %s -- | FileCheck -match-full-lines --check-prefixes="CHECK_NOALL" %s
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

typedef unsigned long size_t;
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

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
