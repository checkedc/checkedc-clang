// RUN: CConvertStandalone %s -- | FileCheck -match-full-lines %s

void foo() {
  int m = 2;
  int *s = &m;
  int q[5] = { 0 };
  int *p = (int *)5;
  p = q + 3;
}
//CHECK: int q[5] = { 0 };
//CHECK-NEXT: int *p = (int *)5;
//CHECK-NEXT: p = q + 3;
