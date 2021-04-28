// RUN: rm -rf %t*
// RUN: 3c -dump-stats -base-dir=%S -alltypes -addcr %s -- 2>&1 | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s


//CHECK: Summary
//CHECK: TotalConstraints|TotalPtrs|TotalNTArr|TotalArr|TotalWild
//CHECK: 18|14|0|1|3
//CHECK: NumPointersNeedBounds:1,
//CHECK: NumNTNoBounds:0,
//CHECK: Details:
//CHECK: Invalid:0
//CHECK: ,BoundsFound:
//CHECK: Array Bounds Inference Stats:
//CHECK: NamePrefixMatch:0
//CHECK: AllocatorMatch:0
//CHECK: VariableNameMatch:0
//CHECK: NeighbourParamMatch:0
//CHECK: DataflowMatch:0
//CHECK: Declared:1


#include <stdlib.h>

void f(int *(*fp)(int *));

void f(int *(*fp)(int *)) {
  //CHECK: void f(_Ptr<_Ptr<int> (_Ptr<int>)> fp) _Checked {
  fp(0);
}
int *g2(int *x) {
  //CHECK: _Ptr<int> g2(_Ptr<int> x) _Checked {
  return x;
}
int *g(int *x: itype(_Ptr<int>)) {
  //CHECK: _Ptr<int> g(_Ptr<int> x) _Checked {
  return 0;
}
void h() {
  int *(*fp)(int *) = g;
  char arr[20];
  //CHECK: _Ptr<_Ptr<int> (_Ptr<int>)> fp = g;
  f(g);
  f(g2);
  int *x = (int *)5;
  //CHECK: int *x = (int *)5;
  g(x);
  //CHECK: g(_Assume_bounds_cast<_Ptr<int>>(x));
}

void i(int *x) {
//CHECK: void i(int *x : itype(_Ptr<int>)) {
  x = 1;
}

void j() {
  void (*fp)(int *) = i;
  //CHECK: _Ptr<void (int * : itype(_Ptr<int>))> fp = i;
}
