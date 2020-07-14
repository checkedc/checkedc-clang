// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

#define NULL ((void*)0)
typedef unsigned long size_t;
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
  char *y;
};
//CHECK: _Ptr<int> x;
//CHECK-NEXT: _Ptr<char> y;

struct r {
  int data;
  struct r *next;
};

struct np *sus(struct r *, struct r *);
//CHECK: struct np *sus(struct r *x : itype(_Ptr<struct r>), struct r *y : itype(_Ptr<struct r>)) : itype(_Ptr<struct np>);

struct r *foo() {
  struct r *x, *y;
  x->data = 2;
  y->data = 1;
  x->next = y;
  y->next = x;
  struct r *z = (struct r *) sus(x, y);
  return z;
}

struct np *bar() {
  struct r *x, *y;
  x->data = 2;
  y->data = 1;
  x->next = y;
  y->next = x;
  struct np *z = sus(x, y);
  return z;
}
//CHECK: _Ptr<struct np> bar(void) {

struct np *sus(struct r *x, struct r *y) {
  x->next += 1;
  struct np *z = malloc(sizeof(struct np));
  z->x = 1;
  z->y = 0;
  return z;
}
//CHECK: struct np *sus(struct r *x : itype(_Ptr<struct r>), struct r *y : itype(_Ptr<struct r>)) : itype(_Ptr<struct np>) {
//CHECK: _Ptr<struct np> z =  malloc<struct np>(sizeof(struct np));
