// RUN: CConvertStandalone %s -- | FileCheck -match-full-lines %s

static int *f(int *x) {
  return x;
}

//CHECK: static _Ptr<int> f(_Ptr<int> x) {
