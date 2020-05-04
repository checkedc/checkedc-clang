// RUN: CConvertStandalone %s -- | FileCheck -match-full-lines %s

static int * f(int *x) {
  x = (int *)5;
  return x;
}
//CHECK: static int * f(int *x) {
