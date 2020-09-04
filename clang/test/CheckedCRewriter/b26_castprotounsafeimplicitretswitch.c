// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/b26_castprotounsafeimplicitretswitch.checked.c -- | count 0
// RUN: rm %S/b26_castprotounsafeimplicitretswitch.checked.c
#include <stddef.h>
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
	//CHECK: char * foo(void) {
  int sx = 3, sy = 4; 
  int *x = &sx;
	//CHECK: int *x = &sx;
  int *y = &sy;
	//CHECK: _Ptr<int> y =  &sy;
  char *z = (int *) sus(x, y);
	//CHECK: char *z = (int *) sus(x, y);
  *z = *z + 1;
  return z;
}

int* bar() {
	//CHECK: int * bar(void) {
  int sx = 3, sy = 4; 
  int *x = &sx;
	//CHECK: int *x = &sx;
  int *y = &sy;
	//CHECK: _Ptr<int> y =  &sy;
  int *z = (sus(x, y));
	//CHECK: int *z = (sus(x, y));
  return z;
}

char *sus(int *x, int*y) {
	//CHECK_NOALL: char *sus(int *x, _Ptr<int> y) : itype(_Ptr<char>) {
	//CHECK_ALL: char *sus(int *x : itype(_Array_ptr<int>), _Ptr<int> y) : itype(_Ptr<char>) {
  char *z = malloc(sizeof(char));
	//CHECK: _Ptr<char> z =  malloc<char>(sizeof(char));
  *z = 1;
  x++;
  *x = 2;
  return z;
}