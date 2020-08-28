// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
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


struct r *sus(struct r x, struct r y) {
	//CHECK: _Ptr<struct r> sus(struct r x, struct r y) {
  x.next += 1;
  struct r *z = malloc(sizeof(struct r));
	//CHECK: _Ptr<struct r> z =  malloc<struct r>(sizeof(struct r));
  z->data = 1;
  z->next = NULL;
  return z;
}

struct r *foo() {
	//CHECK: _Ptr<struct r> foo(void) {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct r *z = (struct r *) sus(x, y);
	//CHECK: _Ptr<struct r> z =  (struct r *) sus(x, y);
  return z;
}

struct r *bar() {
	//CHECK: _Ptr<struct r> bar(void) {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct r *z = sus(x, y);
	//CHECK: _Ptr<struct r> z =  sus(x, y);
  return z;
}