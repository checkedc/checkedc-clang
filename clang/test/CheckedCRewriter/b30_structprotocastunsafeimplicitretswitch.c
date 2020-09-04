// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/b30_structprotocastunsafeimplicitretswitch.checked.c -- | count 0
// RUN: rm %S/b30_structprotocastunsafeimplicitretswitch.checked.c
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
	//CHECK: _Ptr<int> x;
    char *y;
	//CHECK: _Ptr<char> y;
};


struct r {
    int data;
    struct r *next;
	//CHECK: struct r *next;
};


struct np *sus(struct r *, struct r *);
	//CHECK: struct np *sus(struct r *x : itype(_Ptr<struct r>), struct r *y : itype(_Ptr<struct r>)) : itype(_Ptr<struct np>);

struct np *foo() {
	//CHECK: struct np * foo(void) {
  struct r *x;
	//CHECK: struct r *x;
  struct r *y;
	//CHECK: struct r *y;
  x->data = 2;
  y->data = 1;
  x->next = &y;
  y->next = &x;
  struct np *z = (struct r *) sus(x, y);
	//CHECK: struct np *z = (struct r *) sus(x, y);
  return z;
}

struct r *bar() {
	//CHECK: struct r * bar(void) {
  struct r *x; 
	//CHECK: struct r *x; 
  struct r *y;
	//CHECK: struct r *y;
  x->data = 2;
  y->data = 1;
  x->next = &y;
  y->next = &x;
  struct r *z = sus(x, y);
	//CHECK: struct r *z = sus(x, y);
  return z;
}

struct np *sus(struct r *x, struct r *y) {
	//CHECK: struct np *sus(struct r *x : itype(_Ptr<struct r>), struct r *y : itype(_Ptr<struct r>)) : itype(_Ptr<struct np>) {
  x->next += 1;
  struct np *z = malloc(sizeof(struct np));
	//CHECK: _Ptr<struct np> z =  malloc<struct np>(sizeof(struct np));
  z->x = 1;
  z->y = 0;
  return z;
}