// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

int f(int *p);
void bar() {
  int (*fp)(int *p) = f;
  f((void*)0);
}
// CHECK:   _Ptr<int (int *)> fp =  f;

