// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" %s
//RUN: cconv-standalone -output-postfix=checkedNOALL %s
//RUN: %clang -c %S/b26_castprotounsafeimplicitretswitch.checkedNOALL.c
//RUN: rm %S/b26_castprotounsafeimplicitretswitch.checkedNOALL.c

typedef unsigned long size_t;
#define NULL ((void*)0)
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

char *sus(int *, int *);
//CHECK_NOALL: char *sus(int *x, _Ptr<int> y) : itype(_Ptr<char>);
//CHECK_ALL: char *sus(int *x : itype(_Array_ptr<int>), _Ptr<int> y) : itype(_Ptr<char>);


char* foo() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  char *z = (int *) sus(x, y);
  *z = *z + 1;
  return z;
}
//CHECK_NOALL: char* foo() {
//CHECK_NOALL: int *x = &sx;
//CHECK_NOALL: _Ptr<int> y = &sy;
//CHECK_NOALL:   char *z = (int *) sus(x, y);
//CHECK_ALL: char* foo() {
//CHECK_ALL: int *x = &sx;
//CHECK_ALL: _Ptr<int> y = &sy;
//CHECK_ALL:   char *z = (int *) sus(x, y);


int* bar() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = (sus(x, y));
  return z;
}
//CHECK_NOALL: int* bar() {
//CHECK_NOALL: int *x = &sx;
//CHECK_NOALL: _Ptr<int> y = &sy;
//CHECK_NOALL:   int *z = (sus(x, y));
//CHECK_ALL: int* bar() {
//CHECK_ALL: int *x = &sx;
//CHECK_ALL: _Ptr<int> y = &sy;
//CHECK_ALL:   int *z = (sus(x, y));


char *sus(int *x, int*y) {
  char *z = malloc(sizeof(char));
  *z = 1;
  x++;
  *x = 2;
  return z;
}
//CHECK_NOALL: char *sus(int *x, _Ptr<int> y) : itype(_Ptr<char>) {
//CHECK_NOALL:   _Ptr<char> z =  malloc<char>(sizeof(char));
//CHECK_ALL: char *sus(int *x : itype(_Array_ptr<int>), _Ptr<int> y) : itype(_Ptr<char>) {
//CHECK_ALL:   _Ptr<char> z =  malloc<char>(sizeof(char));
