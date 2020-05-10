// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

int *sus(int *x, int*y) {
  int *z = malloc(sizeof(int));
  *z = 1;
  x++;
  *x = 2;
  return z;
}
//CHECK: int *sus(int *x, _Ptr<int> y) : itype(_Ptr<int>) {
//CHECK:   _Ptr<int> z =  malloc(sizeof(int));

int* foo() {
  int sx = 3;
  int sy = 4;
  int *x = &sx; 
  int *y = &sy;
  int *z = sus(x, y);
  *z = *z + 1;
  return z;
}
//CHECK: _Ptr<int> foo(void) {
//CHECK: int *x = &sx;
//CHECK: _Ptr<int> y = &sy;
//CHECK: _Ptr<int> z =  sus(x, y);

int* bar() {
  int sx = 3;
  int sy = 4;
  int *x = &sx; 
  int *y = &sy;
  int *z = sus(x, y);
  z += 2;
  *z = -17;
  return z;
}
//CHECK: int* bar() {
//CHECK: _Ptr<int> y = &sy;
