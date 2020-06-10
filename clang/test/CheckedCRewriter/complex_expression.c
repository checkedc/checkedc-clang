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
void foo(int *y, int *w) {
  int *z = 0;
  z = (y, w = bar(w));
}
//CHECK: int * bar(int *x) { x = (int*)5; return x; }
//CHECK-NEXT: void foo(_Ptr<int> y, int *w) {
//CHECK-NEXT: _Ptr<int> z =  0;
