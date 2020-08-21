// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone -addcr -alltypes -output-postfix=checked %s 
// RUN: cconv-standalone -addcr -alltypes %S/pointerarithm.checked.c -- | count 0
// RUN: rm %S/pointerarithm.checked.c

#define NULL ((void*)0)
typedef unsigned long size_t;
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

int *sus(int *x, int*y) {
  int *z = malloc(sizeof(int)*2);
  *z = 1;
  x++;
  *x = 2;
  return z;
}
//CHECK: _Array_ptr<int> sus(int *x : itype(_Array_ptr<int>), _Ptr<int> y) : count(2) {
//CHECK-NEXT:  _Array_ptr<int> z : count(2) =  malloc<int>(sizeof(int)*2);

int* foo() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = sus(x, y);
  *z = *z + 1;
  return z;
}
//CHECK: _Ptr<int> foo(void) {
//CHECK: _Ptr<int> y = &sy;
//CHECK: _Ptr<int> z =  sus(x, y);


int* bar() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = sus(x, y) + 2;
  *z = -17;
  return z;
}
//CHECK: _Ptr<int> bar(void) {
//CHECK: _Ptr<int> y = &sy;
//CHECK: _Ptr<int> z =  sus(x, y) + 2;

