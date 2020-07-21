// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

typedef unsigned long size_t;
#define NULL 0
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

int* sus(int *, int *);
//CHECK_NOALL: int *sus(int *x, _Ptr<int> y) : itype(_Ptr<int>);
//CHECK_ALL: int * sus(int *x : itype(_Array_ptr<int>), _Ptr<int> y);


int* foo() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = sus(x, y);
  *z = *z + 1;
  return z;
}
//CHECK_NOALL: _Ptr<int> foo(void) {
//CHECK_NOALL:   _Ptr<int> z =  sus(x, y);
//CHECK_ALL: int * foo(void) {
//CHECK: int *x = &sx;
//CHECK: _Ptr<int> y = &sy;
//CHECK_ALL:   int *z = sus(x, y);


int* bar() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = (sus(x, y));
  z += 2;
  return z;
}
//CHECK: int * bar(void) {
//CHECK: int *x = &sx;
//CHECK: _Ptr<int> y = &sy;
//CHECK:   int *z = (sus(x, y));


int *sus(int *x, int*y) {
  int *z = malloc(sizeof(int));
  *z = 1;
  x++;
  *x = 2;
  return z;
}
//CHECK_NOALL: int *sus(int *x, _Ptr<int> y) : itype(_Ptr<int>) {
//CHECK_NOALL:   _Ptr<int> z =  malloc<int>(sizeof(int));
//CHECK_ALL: int * sus(int *x : itype(_Array_ptr<int>), _Ptr<int> y) {
//CHECK_ALL:   int *z = malloc<int>(sizeof(int));
