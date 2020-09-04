// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/return_not_least.checked.c -- | count 0
// RUN: rm %S/return_not_least.checked.c

int *a() {
	//CHECK_NOALL: int *a(void) {
	//CHECK_ALL: _Array_ptr<int> a(void) _Checked {
  int *a = 0;
	//CHECK_NOALL: int *a = 0;
	//CHECK_ALL:   _Array_ptr<int> a =  0;
  return a++;
}

int *dumb(int *a){
	//CHECK: _Ptr<int> dumb(_Ptr<int> a)_Checked {
  int *b = a;
	//CHECK: _Ptr<int> b =  a;
  return b;
}

int *f(void) {
	//CHECK_NOALL: int *f(void) {
	//CHECK_ALL: _Array_ptr<int> f(void) {
  int *p = (int*)0;
	//CHECK_NOALL: int *p = (int*)0;
	//CHECK_ALL:   _Array_ptr<int> p =  (_Array_ptr<int>)0;
  p++;
  return p;
}

int *foo(void) {
	//CHECK_NOALL: int *foo(void) {
	//CHECK_ALL: _Array_ptr<int> foo(void) _Checked {
  int *q = f();
	//CHECK_NOALL: int *q = f();
	//CHECK_ALL:   _Array_ptr<int> q =  f();
  return q;
}

#include <stddef.h>
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);

int *bar() {
	//CHECK_NOALL: int *bar(void) {
	//CHECK_ALL: _Array_ptr<int> bar(void) {
  int *z = calloc(2, sizeof(int));
	//CHECK_NOALL: int *z = calloc<int>(2, sizeof(int));
	//CHECK_ALL:   _Array_ptr<int> z : count(2) =  calloc<int>(2, sizeof(int));
  z += 2;
  return z;
}

int *baz(int *a) {
	//CHECK_NOALL: int *baz(int *a) {
	//CHECK_ALL: _Array_ptr<int> baz(_Array_ptr<int> a) {
  a++;

  int *b = (int*) 0;
	//CHECK_NOALL: int *b = (int*) 0;
	//CHECK_ALL:   _Array_ptr<int> b =  (_Array_ptr<int>) 0;
  a = b;

  int *c = b;
	//CHECK_NOALL: int *c = b;
	//CHECK_ALL:   _Array_ptr<int> c =  b;

  return c;
}

int *buz(int *a) {
	//CHECK_NOALL: int *buz(int *a) {
	//CHECK_ALL: _Ptr<int> buz(_Array_ptr<int> a) {
  a++;

  int *b = (int*) 0;
	//CHECK_NOALL: int *b = (int*) 0;
	//CHECK_ALL:   _Array_ptr<int> b =  (_Array_ptr<int>) 0;
  a = b;

  /* The current implementation does not propagate array constraint to c and d, but
   if this test starts failing because it does, that's probably OK. */

  int *c = b;
	//CHECK_NOALL: int *c = b;
	//CHECK_ALL:   _Ptr<int> c =  b;

  int *d = (int*) 0;
	//CHECK_NOALL: int *d = (int*) 0;
	//CHECK_ALL:   _Ptr<int> d =  (_Ptr<int>) 0;
  c = d;

  return d;
}
