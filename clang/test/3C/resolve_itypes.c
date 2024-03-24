// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/resolve_itypes.c -- | diff %t.checked/resolve_itypes.c -

void foo(int *a : itype(_Ptr<int>)) {
//CHECK_NOALL: void foo(int *a : itype(_Ptr<int>)) {
//CHECK_ALL: void foo(int *a : itype(_Array_ptr<int>) count(0 + 1)) {
  a = (int*) 1;
  (void) a[0];
}

_Array_ptr<int> b = 0;
int *bar(void) : itype(_Ptr<int>) {
//CHECK_NOALL: int *bar(void) : itype(_Ptr<int>) {
//CHECK_ALL: int *bar(void) : itype(_Array_ptr<int>) {
  return 0 ? (int*) 1 : b;
}

void nt_fn(int * x : itype(_Nt_array_ptr<int>));
//CHECK: void nt_fn(int * x : itype(_Nt_array_ptr<int>));
void baz(int *c : itype(_Array_ptr<int>)) {
//CHECK_ALL: void baz(int *c : itype(_Nt_array_ptr<int>)) {
//CHECK_NOALL: void baz(int *c : itype(_Ptr<int>)) {
  c = (int *) 1;
  nt_fn(c);
}

void arr_fn(int *x: itype(_Array_ptr<int>));
//CHECK: void arr_fn(int *x: itype(_Array_ptr<int>));
void buz(int *a : itype(_Ptr<int>)) _Checked {
//CHECK_NOALL: void buz(int *a : itype(_Ptr<int>)) _Checked {
//CHECK_ALL: void buz(_Array_ptr<int> a) _Checked  {
  arr_fn(a);
}

int *biz(void) : itype(_Ptr<int>) {
//CHECK_NOALL: int *biz(void) : itype(_Ptr<int>) _Checked {
//CHECK_ALL: _Array_ptr<int> biz(void) _Checked {
  return b;
}

void fuz(int *a : itype(_Ptr<int>)) {}
//CHECK: void fuz(_Ptr<int> a) _Checked {}

#include <stdlib.h>
extern _Itype_for_any(T) void paper_foo(void * : itype(_Array_ptr<T>));
void paper_bar(int *q : itype(_Ptr<int>), int n) {
//CHECK_ALL: void paper_bar(_Array_ptr<int> q : count(n), int n) {
//CHECK_NOALL: void paper_bar(int *q : itype(_Ptr<int>), int n) {
  q = malloc<int>(sizeof(int)*n);
  paper_foo(q);
}

static int static_foo(int *a : itype(_Array_ptr<int>) count(4)) {
//CHECK_ALL: static int static_foo(_Array_ptr<int> a : count(4)) _Checked {
  return *a;
}
