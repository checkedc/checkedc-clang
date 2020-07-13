// Tests for Checked C rewriter tool.
//
// Tests cconv-standalone tool for complex expressions
//
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
//

#define NULL ((void *)0)
typedef unsigned long size_t;

int * func(int *a, int *b) {
    // This is a checked pointer
    return *a?(2+0):b;	
}
//CHECK: int * func(_Ptr<int> a, int *b) {

int main() {
  int *arr;
  int *c;
  int *b;
  b = (c = func(arr+3, 2+2)) ? 0: 2;
  return 0;
}
//CHECK: _Array_ptr<int> arr = ((void *)0);
//CHECK-NEXT: int *c;
//CHECK-NEXT: int *b;

int * bar(int *x) { x = (int*)5; return x; }
int *foo(int *y, int *w) {
  int *z = 0;
  z = (w = bar(w), y);
  return z;
}
//CHECK: int * bar(int *x) { x = (int*)5; return x; }
//CHECK-NEXT: _Array_ptr<int> foo(_Array_ptr<int> y, int *w) {
//CHECK-NEXT: _Array_ptr<int> z =  0;

void baz(int *p) {
  int *q = 0 ? p : foo(0,0);
  q++;
}
//CHECK: void baz(_Array_ptr<int> p) {
//CHECK-NEXT:  _Array_ptr<int> q =  0 ? p : foo(0,0);

void test() {
  int *a = (int*) 0;
  int **b = (int*) 0;

  *b = (0, a);
}
//CHECK: _Ptr<int> a = (int*) 0;
//CHECK: _Ptr<_Ptr<int>> b = (int*) 0;
