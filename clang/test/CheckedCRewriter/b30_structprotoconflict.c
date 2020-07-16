// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" %s
//RUN: cconv-standalone -output-postfix=checkedNOALL %s
//RUN: %clang -c %S/b30_structprotoconflict.checkedNOALL.c
//RUN: rm %S/b30_structprotoconflict.checkedNOALL.c

typedef unsigned long size_t;
#define NULL ((void*)0)
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

struct r {
  int data;
  struct r *next;
};

struct r *sus(struct r *, struct r *);
//CHECK_NOALL: struct r *sus(_Ptr<struct r> x, _Ptr<struct r> y) : itype(_Ptr<struct r>);
//CHECK_ALL: struct r *sus(_Ptr<struct r> x, _Ptr<struct r> y) : itype(_Ptr<struct r>);


struct np *foo() {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct np *z = (struct np *) sus(&x, &y);
  return z;
}
//CHECK_NOALL: struct np *foo() {
//CHECK_NOALL:   struct np *z = (struct np *) sus(&x, &y);
//CHECK_ALL: struct np *foo() {
//CHECK_ALL:   struct np *z = (struct np *) sus(&x, &y);


// FIXME-- the above annotation is wrong; the cast at bar() is making it seem like it should be an itype

struct r *bar() {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct r *z = (struct r *) sus(&x, &y);
  return z;
}
//CHECK_NOALL: // FIXME-- the above annotation is wrong; the cast at bar() is making it seem like it should be an itype
//CHECK_NOALL: _Ptr<struct r> bar(void) {
//CHECK_NOALL:   _Ptr<struct r> z =  (struct r *) sus(&x, &y);
//CHECK_ALL: // FIXME-- the above annotation is wrong; the cast at bar() is making it seem like it should be an itype
//CHECK_ALL: _Ptr<struct r> bar(void) {
//CHECK_ALL:   _Ptr<struct r> z =  (struct r *) sus(&x, &y);


struct r *sus(struct r *x, struct r *y) {
  x->next += 1;
  struct r *z = malloc(sizeof(struct r));
  z->data = 1;
  z->next = 0;
  return z;
}
//CHECK_NOALL: struct r *sus(_Ptr<struct r> x, _Ptr<struct r> y) : itype(_Ptr<struct r>) {
//CHECK_NOALL:   _Ptr<struct r> z =  malloc<struct r>(sizeof(struct r));
//CHECK_ALL: struct r *sus(_Ptr<struct r> x, _Ptr<struct r> y) : itype(_Ptr<struct r>) {
//CHECK_ALL:   _Ptr<struct r> z =  malloc<struct r>(sizeof(struct r));
