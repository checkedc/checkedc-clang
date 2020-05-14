// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

void foo(int *x) {
  x = (int *)5;
  int **y = &x;
}
//CHECK: int **y = &x;

void bar(int *x) {
  x = (int *)5;
  int *y = *(&x);
}

int f(int *x) {
  return *x;
}
//CHECK: int f(_Ptr<int> x) {

void baz(void) {
  int (*fp)(int *) = f;
  int (*fp2)(int *) = &f;
  f((void*)0);
}
//CHECK:  _Ptr<int (_Ptr<int> )> fp =  f;
//CHECK:  _Ptr<int (_Ptr<int> )> fp2 =  &f;
