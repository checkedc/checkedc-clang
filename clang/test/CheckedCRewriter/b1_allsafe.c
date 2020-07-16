// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" %s
//RUN: cconv-standalone -output-postfix=checkedNOALL %s
//RUN: %clang -c %S/b1_allsafe.checkedNOALL.c
//RUN: rm %S/b1_allsafe.checkedNOALL.c

typedef unsigned long size_t;
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

int *sus(int *x, int*y) {
  int *z = malloc(sizeof(int));
  *z = 1;
  x++;
  *x = 2;
  return z;
}
//CHECK_NOALL: _Ptr<int> sus(int *x, _Ptr<int> y) {
//CHECK_NOALL:   _Ptr<int> z =  malloc<int>(sizeof(int));
//CHECK_ALL: _Ptr<int> sus(int *x : itype(_Array_ptr<int>), _Ptr<int> y) {
//CHECK_ALL:   _Ptr<int> z =  malloc<int>(sizeof(int));


int* foo() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = sus(x, y);
  *z = *z + 1;
  return z;
}
//CHECK_NOALL: _Ptr<int> foo(void) {
//CHECK_NOALL: int *x = &sx;
//CHECK_NOALL: _Ptr<int> y = &sy;
//CHECK_NOALL:   _Ptr<int> z =  sus(x, y);
//CHECK_ALL: _Ptr<int> foo(void) {
//CHECK_ALL: int *x = &sx;
//CHECK_ALL: _Ptr<int> y = &sy;
//CHECK_ALL:   _Ptr<int> z =  sus(x, y);


int* bar() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = (sus(x, y));
  return z;
}
//CHECK_NOALL: _Ptr<int> bar(void) {
//CHECK_NOALL: int *x = &sx;
//CHECK_NOALL: _Ptr<int> y = &sy;
//CHECK_NOALL:   _Ptr<int> z =  (sus(x, y));
//CHECK_ALL: _Ptr<int> bar(void) {
//CHECK_ALL: int *x = &sx;
//CHECK_ALL: _Ptr<int> y = &sy;
//CHECK_ALL:   _Ptr<int> z =  (sus(x, y));
