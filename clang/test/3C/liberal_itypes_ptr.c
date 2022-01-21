// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/liberal_itypes_ptr.c -- | diff %t.checked/liberal_itypes_ptr.c -

void foo(int *a) {}
// CHECK: void foo(_Ptr<int> a) _Checked {}

void bar() {
  int *b = 1;
  // CHECK: int *b = 1;
  foo(b);
  // CHECK: foo(_Assume_bounds_cast<_Ptr<int>>(b));
}

void baz() {
  int *b = 0;
  // CHECK: _Ptr<int> b = 0;
  foo(b);
  // CHECK: foo(b);
}

int *buz() { return 0; }
// CHECK: _Ptr<int> buz(void) _Checked { return 0; }

void boz() {
  // CHECK: void boz() {
  int *b = 1;
  // CHECK: int *b  = 1;
  b = buz();
  // CHECK: b = ((int *)buz());
}

void biz() {
  // CHECK: void biz() _Checked {
  int *b = 0;
  // CHECK: _Ptr<int> b = 0;
  b = buz();
  // CHECK: b = buz();
}

#include <stdlib.h>

void malloc_test() {
  int *p = malloc(sizeof(int));
  // CHECK: int *p = malloc<int>(sizeof(int));
  p = 1;
}

void free_test() {
  int *a;
  // CHECK: _Ptr<int> a = ((void *)0);
  free(a);
  // CHECK: free<int>(a);
}

void unsafe(int *a) {
  // CHECK: void unsafe(int *a : itype(_Ptr<int>)) {
  a = 1;
}

int *unsafe_return() {
  // CHECK: int *unsafe_return(void) : itype(_Ptr<int>) _Checked {
  return 1;
}

void caller() {
  int *b;
  // CHECK: _Ptr<int> b = ((void *)0);
  unsafe(b);
  // CHECK: unsafe(b);

  int *c = 1;
  // CHECK: int *c = 1;
  unsafe(c);
  // CHECK: unsafe(c);

  int *d = unsafe_return();
  // CHECK: _Ptr<int> d = unsafe_return();

  int *e = unsafe_return();
  // CHECK: int *e = unsafe_return();
  e = 1;
}

void checked(_Ptr<int> i) {}
// CHECK: void checked(_Ptr<int> i) _Checked {}

void itype_unsafe(int *i : itype(_Ptr<int>)) { i = 1; }
// CHECK: void itype_unsafe(int *i : itype(_Ptr<int>)) { i = 1; }

void itype_safe(int *i : itype(_Ptr<int>)) { i = 0; }
// CHECK: void itype_safe(_Ptr<int> i) _Checked { i = 0; }

void void_ptr(void *p, void *q) {
  // CHECK: void void_ptr(void *p, void *q) {
  p = 1;
}

void int_ptr_arg(int *a) {}
// CHECK: void int_ptr_arg(_Ptr<int> a) _Checked {}

void char_ptr_param() {
  int_ptr_arg((char *)1);
  // CHECK: int_ptr_arg(_Assume_bounds_cast<_Ptr<int>>((char *)1));

  int *c;
  // CHECK: _Ptr<int> c = ((void *)0);
  int_ptr_arg(c);
  // CHECK: int_ptr_arg(c);
}

void bounds_fn(void *b : byte_count(1));
// CHECK: void bounds_fn(void *b : byte_count(1));

void bounds_call(void *p) {
  // CHECK_NOALL: void bounds_call(void *p) {
  // CHECK_ALL: _Itype_for_any(T) void bounds_call(_Array_ptr<T> p : byte_count(1)) {
  bounds_fn(p);
  // CHECK: bounds_fn(p);
}

char *unused_return_unchecked();
char *unused_return_checked() { return 0; }
char *unused_return_itype() { return 1; }
char **unused_return_unchecked_ptrptr();
//CHECK: char *unused_return_unchecked();
//CHECK: _Ptr<char> unused_return_checked(void) _Checked { return 0; }
//CHECK: char *unused_return_itype(void) : itype(_Ptr<char>) _Checked { return 1; }
//CHECK: char **unused_return_unchecked_ptrptr();

void dont_cast_unused_return() {
  unused_return_unchecked();
  *unused_return_unchecked();
  (void)unused_return_unchecked();
  //CHECK: unused_return_unchecked();
  //CHECK: *unused_return_unchecked();
  //CHECK: (void)unused_return_unchecked();

  unused_return_checked();
  *unused_return_checked();
  (void)unused_return_checked();
  //CHECK: unused_return_checked();
  //CHECK: *unused_return_checked();
  //CHECK: (void)unused_return_checked();

  unused_return_itype();
  *unused_return_itype();
  (void)unused_return_itype();
  //CHECK: unused_return_itype();
  //CHECK: *unused_return_itype();
  //CHECK: (void)unused_return_itype();

  unused_return_unchecked_ptrptr();
  *unused_return_unchecked_ptrptr();
  (void)unused_return_unchecked_ptrptr();
  //CHECK: unused_return_unchecked_ptrptr();
  //CHECK: *unused_return_unchecked_ptrptr();
  //CHECK: (void)unused_return_unchecked_ptrptr();
}
