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
