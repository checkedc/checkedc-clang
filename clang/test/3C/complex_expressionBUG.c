// Tests for 3C.
//
// Tests 3c tool for complex expressions
// The following test is supposed to fail with the current tool.
// XFAIL: *
// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK","CHECK_NEXT" %s
// RUN: 3c %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK","CHECK_NEXT" %s
// RUN: 3c %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stddef.h>

int * func(int *a, int *b) {
    // This is a checked pointer
    return *a?(2+0):b;	
}
//CHECK_ALL: int * func(_Ptr<int> a, int *b) {
//CHECK_NOALL: int * func(int *a : itype(_Ptr<int>), int *b) { 

int main() {
  int *arr;
  int *c;
  int *b;
  b = (c = func(arr+3, 2+2)) ? 0: 2;
  return 0;
}
//CHECK_ALL: _Array_ptr<int> arr = ((void *)0);
//CHECK: int *c;
//CHECK-NEXT: int *b;

int * bar(int *x) { x = (int*)5; return x; }
int *foo(int *y, int *w) {
  int *z = 0;
  z = (w = bar(w), y);
  return z;
}
//CHECK: int * bar(int *x) { x = (int*)5; return x; }
//CHECK_ALL: _Array_ptr<int> foo(_Array_ptr<int> y, int *w) {
//CHECK_ALL: _Array_ptr<int> z =  0;
//CHECK_NOALL: int *foo(_Ptr<int> y, int *w) : itype(_Ptr<int>) { 
//CHECK_NOALL: _Ptr<int> z =  0;

void baz(int *p) {
  int *q = 0 ? p : foo(0,0);
  q++;
}
//CHECK_ALL: void baz(_Array_ptr<int> p) {
//CHECK_NOALL: void baz(int *p) {
//CHECK_ALL:  _Array_ptr<int> q =  0 ? p : foo(0,0);
//CHECK_NOALL: int *q = 0 ? p : foo(0,0);

void test() {
  int *a = (int*) 0;
  int **b = (int**) 0; /* <-- causes compilation failure due to issue 160*/

  *b = (0, a);
}
//CHECK: _Ptr<int> a = (int*) 0;
//CHECK: _Ptr<_Ptr<int>> b = (int**) 0;
