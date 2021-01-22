// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/b8_allsafestructnp.checked.c -- | count 0
// RUN: rm %S/b8_allsafestructnp.checked.c
#include <stddef.h>
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));


struct np {
    int x;
    int y;
};

struct p {
    int *x;
	//CHECK: int *x;
    char *y;
	//CHECK: char *y;
};


struct r {
    int data;
    struct r *next;
	//CHECK: _Ptr<struct r> next;
};


struct np *sus(struct p x, struct p y) {
	//CHECK: _Ptr<struct np> sus(struct p x, struct p y) {
  struct np *z = malloc(sizeof(struct np));
	//CHECK: _Ptr<struct np> z =  malloc<struct np>(sizeof(struct np));
  z->x = 1;
  z->x = 2;
  return z;
}

struct np *foo() {
	//CHECK: _Ptr<struct np> foo(void) {
  struct p x, y;
  x.x = 1;
  x.y = 2;
  y.x = 3;
  y.y = 4;
  struct np *z = sus(x, y);
	//CHECK: _Ptr<struct np> z =  sus(x, y);
  return z;
}

struct np *bar() {
	//CHECK: _Ptr<struct np> bar(void) {
  struct p x, y;
  x.x = 1;
  x.y = 2;
  y.x = 3;
  y.y = 4;
  struct np *z = sus(x, y);
	//CHECK: _Ptr<struct np> z =  sus(x, y);
  return z;
}
