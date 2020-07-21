// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

typedef unsigned long size_t;
#define NULL 0
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
//CHECK:     int *x;
//CHECK:     char *y;


struct r {
    int data;
    struct r *next;
};
//CHECK:     _Ptr<struct r> next;


struct np *sus(struct p x, struct p y) {
  struct np *z = malloc(sizeof(struct np));
  z->x = 1;
  z->x = 2;
  return z;
}
//CHECK_NOALL: struct np *sus(struct p x, struct p y) : itype(_Ptr<struct np>) {
//CHECK_NOALL:   _Ptr<struct np> z =  malloc<struct np>(sizeof(struct np));
//CHECK_ALL: struct np * sus(struct p x, struct p y) {
//CHECK_ALL:   struct np *z = malloc<struct np>(sizeof(struct np));


struct np *foo() {
  struct p x, y;
  x.x = 1;
  x.y = 2;
  y.x = 3;
  y.y = 4;
  struct np *z = sus(x, y);
  return z;
}
//CHECK_NOALL: _Ptr<struct np> foo(void) {
//CHECK_NOALL:   _Ptr<struct np> z =  sus(x, y);
//CHECK_ALL: struct np * foo(void) {
//CHECK_ALL:   struct np *z = sus(x, y);


struct np *bar() {
  struct p x, y;
  x.x = 1;
  x.y = 2;
  y.x = 3;
  y.y = 4;
  struct np *z = sus(x, y);
  z += 2;
  return z;
}
//CHECK: struct np * bar(void) {
//CHECK:   struct np *z = sus(x, y);
