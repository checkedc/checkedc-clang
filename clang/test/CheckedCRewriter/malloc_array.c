// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/malloc_array.checked.c -- | count 0
// RUN: rm %S/malloc_array.checked.c

typedef unsigned long size_t;
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

#define NULL 0
typedef unsigned long size_t;
_Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
	//CHECK: _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
_Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
	//CHECK: _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
	//CHECK: _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
_Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
	//CHECK: _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

int * foo(int *x) {
	//CHECK_NOALL: int * foo(int *x) {
	//CHECK_ALL: _Array_ptr<int> foo(int *x : itype(_Array_ptr<int>)) _Checked {
  x[2] = 1;
  return x;
}
void bar(void) {
  int *y = malloc(sizeof(int)*2);
	//CHECK: int *y = malloc<int>(sizeof(int)*2);
  y = (int *)5;
	//CHECK: y = (int *)5;
  int *z = foo(y);
	//CHECK_NOALL: int *z = foo(y);
	//CHECK_ALL:   _Ptr<int> z =  foo(y);
} 

void force(int *x){}
	//CHECK: void force(_Ptr<int> x)_Checked {}
