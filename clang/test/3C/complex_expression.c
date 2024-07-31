// Tests for 3C.
//
// Tests 3c tool for complex expressions
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -addcr %t.checked/complex_expression.c -- -Wno-error=int-conversion | diff %t.checked/complex_expression.c -

#include <stddef.h>

int *func(int *a, int *b) {
  // This is a checked pointer
  return *a ? (2 + 0) : b;
}
//CHECK: int *func(_Ptr<int> a, int *b : itype(_Ptr<int>)) : itype(_Ptr<int>) {

int main() {
  int *arr;
  int *c;
  int *b;
  b = (c = func(arr + 3, 2 + 2)) ? 0 : 2;
  return 0;
}
//CHECK_ALL: _Array_ptr<int> arr = ((void *)0);
//CHECK: _Ptr<int> c = ((void *)0);
//CHECK-NEXT: int *b;

int *bar(int *x) {
  x = (int *)5;
  return x;
}
int *foo(int *y, int *w) {
  int *z = 0;
  z = (w = bar(w), y);
  return z;
}
//CHECK:      int *bar(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
//CHECK-NEXT:   x = (int *)5;
//CHECK-NEXT:   return x;
//CHECK-NEXT: }

//CHECK_ALL: _Array_ptr<int> foo(_Array_ptr<int> y, _Ptr<int> w) _Checked {
//CHECK_ALL: _Array_ptr<int> z = 0;
//CHECK_NOALL: _Ptr<int> foo(_Ptr<int> y, _Ptr<int> w) _Checked {
//CHECK_NOALL: _Ptr<int> z = 0;

void baz(int *p) {
  int *q = 0 ? p : foo(0, 0);
  q++;
}
//CHECK_ALL: void baz(_Array_ptr<int> p) _Checked {
//CHECK_NOALL: void baz(int *p : itype(_Ptr<int>)) {
//CHECK_ALL: _Array_ptr<int> q = 0 ? p : foo(0, 0);
//CHECK_NOALL: int *q = 0 ? p : ((int *)foo(0, 0));

void test() {
  int *a = (int *)0;
  int **b = (int **)0;

  *b = (0, a);
}
//CHECK: _Ptr<int> a = (_Ptr<int>)0;
//CHECK: _Ptr<_Ptr<int>> b = (_Ptr<_Ptr<int>>)0;
