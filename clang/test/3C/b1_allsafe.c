// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/b1_allsafe.checked.c -- | count 0
// RUN: rm %S/b1_allsafe.checked.c
#include <stddef.h>
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

int *sus(int *x, int*y) {
	//CHECK_NOALL: _Ptr<int> sus(int *x : itype(_Ptr<int>), _Ptr<int> y) {
	//CHECK_ALL: _Ptr<int> sus(_Array_ptr<int> x, _Ptr<int> y) {
  int *z = malloc(sizeof(int));
	//CHECK: _Ptr<int> z = malloc<int>(sizeof(int));
  *z = 1;
  x++;
  *x = 2;
  return z;
}

int* foo() {
	//CHECK_NOALL: _Ptr<int> foo(void) _Checked {
	//CHECK_ALL: _Ptr<int> foo(void) {
  int sx = 3, sy = 4; 
  int *x = &sx;
	//CHECK_NOALL: _Ptr<int> x = &sx;
	//CHECK_ALL:   int *x = &sx;
  int *y = &sy;
	//CHECK: _Ptr<int> y = &sy;
  int *z = sus(x, y);
	//CHECK_NOALL: _Ptr<int> z = sus(x, y);
	//CHECK_ALL:   _Ptr<int> z = sus(_Assume_bounds_cast<_Array_ptr<int>>(x, byte_count(0)), y);
  *z = *z + 1;
  return z;
}

int* bar() {
	//CHECK_NOALL: _Ptr<int> bar(void) _Checked {
	//CHECK_ALL: _Ptr<int> bar(void) {
  int sx = 3, sy = 4; 
  int *x = &sx;
	//CHECK_NOALL: _Ptr<int> x = &sx;
	//CHECK_ALL:   int *x = &sx;
  int *y = &sy;
	//CHECK: _Ptr<int> y = &sy;
  int *z = (sus(x, y));
	//CHECK_NOALL: _Ptr<int> z = (sus(x, y));
	//CHECK_ALL:   _Ptr<int> z = (sus(_Assume_bounds_cast<_Array_ptr<int>>(x, byte_count(0)), y));
  return z;
}
