// RUN: rm -rf %t*
// RUN: mkdir %t && cd %t
// RUN: 3c -dump-stats -base-dir=%S -alltypes -addcr %s -- 2>stderr | FileCheck -match-full-lines %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_STDERR" --input-file %t/stderr %s


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

//CHECK_STDERR: Summary
//CHECK_STDERR: TotalConstraints|TotalPtrs|TotalNTArr|TotalArr|TotalWild
//CHECK_STDERR: 18|14|0|1|3
//CHECK_STDERR: NumPointersNeedBounds:1,
//CHECK_STDERR: NumNTNoBounds:0,
//CHECK_STDERR: Details:
//CHECK_STDERR: Invalid:0
//CHECK_STDERR: ,BoundsFound:
//CHECK_STDERR: Array Bounds Inference Stats:
//CHECK_STDERR: NamePrefixMatch:0
//CHECK_STDERR: AllocatorMatch:0
//CHECK_STDERR: VariableNameMatch:0
//CHECK_STDERR: NeighbourParamMatch:0
//CHECK_STDERR: DataflowMatch:0
//CHECK_STDERR: Declared:1
//CHECK_STDERR: TimeStats
//CHECK_STDERR: TotalTime:{{.*}}
//CHECK_STDERR: ConstraintBuilderTime:{{.*}}
//CHECK_STDERR: ConstraintSolverTime:{{.*}}
//CHECK_STDERR: ArrayBoundsInferenceTime:{{.*}}
//CHECK_STDERR: RewritingTime:{{.*}}
//CHECK_STDERR: ReWriteStats
//CHECK_STDERR: NumAssumeBoundsCasts:1
//CHECK_STDERR: NumCheckedCasts:0
//CHECK_STDERR: NumWildCasts:0
//CHECK_STDERR: NumFixedCasts:0
//CHECK_STDERR: NumITypes:2
//CHECK_STDERR: NumCheckedRegions:4
//CHECK_STDERR: NumUnCheckedRegions:0
