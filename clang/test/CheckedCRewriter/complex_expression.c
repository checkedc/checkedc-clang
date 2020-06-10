// Tests for Checked C rewriter tool.
//
// Tests cconv-standalone tool for complex expressions
//
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
//

#define NULL ((void *)0)
typedef unsigned long size_t;

int *func(int *a, int *b) {
    // This is a checked pointer
    return *a?(0+0):b;	
}
//CHECK: _Ptr<int> func(int *a : itype(_Ptr<int>), _Ptr<int> b) {

int main() {
  int *arr;
  int *c;
  int *b;
  b = (c = func(2+2, arr + 3)) ? 0: 2;
  return 0;
}
//CHECK: _Array_ptr<int> arr = ((void *)0);
//CHECK-NEXT: _Ptr<int> c = ((void *)0);

int * bar(int *x) { x = (int*)5; return x; }
void foo(int *y, int *w) {
  int *z = 0;
  z = (y, w = bar(w));
}
//CHECK: int * bar(int *x) { x = (int*)5; return x; }
//CHECK-NEXT: void foo(_Ptr<int> y, int *w) {
//CHECK-NEXT: _Ptr<int> z =  0;
