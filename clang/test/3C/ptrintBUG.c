// The following test is supposed to fail with the current tool.
// XFAIL: *
// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// XFAIL: *

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
 p = q; // OK          <----- NOT OK, causes compilation error, please refer to iss160
 int *d = (int *)q;
}
//CHECK: int *p;
//CHECK: int *d = (int *)q;
