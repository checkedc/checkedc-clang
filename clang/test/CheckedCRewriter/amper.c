// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

void foo(int *x) {
  x = (int *)5;
  int **y = &x;
}
//CHECK: _Ptr<int*> y = &x;

void bar(int *x) {
  x = (int *)5;
  int *y = *(&x);
}

int *id(int *x) {
  return &(*x);
}
//CHECK: _Ptr<int> id(_Ptr<int> x) {

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

extern int xfunc(int *arg);
int (*fp)(int *);
//CHECK: _Ptr<int (int *)> fp = ((void *)0);

void addrof(void){
  fp = &xfunc;
}

void bif(int **x) {
  int **w = 0;
  int *y = *(x = w);
  w = &y;
}
//CHECK: void bif(_Ptr<_Ptr<int>> x) {
//CHECK-NEXT: _Ptr<_Ptr<int>> w =  0;
//CHECK-NEXT: _Ptr<int> y =  *(x = w);

