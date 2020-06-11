// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
//

void baz(int *x, int *y, int *z) {
  int p;
  int q = z;
  p = z;
}
//CHECK: void baz(int *x : itype(_Ptr<int>), _Ptr<int> y, _Ptr<int> z) {

void bar(int *x) {
  baz(2,0,x);
}
//CHECK: void bar(_Ptr<int> x) {

void foo(void) {
 int *p;
 int q = 0;
 p = q; // OK
 int *d = (int *)q;
}
//CHECK: int *p;
//CHECK: int *d = (int *)q;
