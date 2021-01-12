// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/b23_retswitchexplicit.checked.c -- | count 0
// RUN: rm %S/b23_retswitchexplicit.checked.c
#include <stddef.h>
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

char *sus(int *x, int*y) {
	//CHECK_NOALL: char *sus(int *x : itype(_Ptr<int>), _Ptr<int> y) : itype(_Ptr<char>) {
	//CHECK_ALL: char *sus(_Array_ptr<int> x, _Ptr<int> y) : itype(_Ptr<char>) {
  int *z = malloc(sizeof(int));
	//CHECK: int *z = malloc<int>(sizeof(int));
  *z = 1;
  x++;
  *x = 2;
  return z;
}

char* foo() {
	//CHECK: char *foo(void) : itype(_Ptr<char>) {
  int sx = 3, sy = 4; 
  int *x = &sx;
	//CHECK_NOALL: _Ptr<int> x = &sx;
	//CHECK_ALL:   int *x = &sx;
  int *y = &sy;
	//CHECK: _Ptr<int> y = &sy;
  char *z = (int *) sus(x, y);
	//CHECK_NOALL: char *z = (int *) sus(x, y);
	//CHECK_ALL:   char *z = (int *) sus(_Assume_bounds_cast<_Array_ptr<int>>(x, byte_count(0)), y);
  *z = *z + 1;
  return z;
}

int* bar() {
	//CHECK: int *bar(void) : itype(_Ptr<int>) {
  int sx = 3, sy = 4; 
  int *x = &sx;
	//CHECK_NOALL: _Ptr<int> x = &sx;
	//CHECK_ALL:   int *x = &sx;
  int *y = &sy;
	//CHECK: _Ptr<int> y = &sy;
  int *z = (char *) (sus(x, y));
	//CHECK_NOALL: int *z = (char *) (sus(x, y));
	//CHECK_ALL:   int *z = (char *) (sus(_Assume_bounds_cast<_Array_ptr<int>>(x, byte_count(0)), y));
  return z;
}
