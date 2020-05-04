// RUN: CConvertStandalone %s -- | FileCheck -match-full-lines %s

int *sus(int *x, int*y) {
  int *z = malloc(sizeof(int));
  *z = 1;
  x++;
  *x = 2;
  z++;
  return z;
}
//CHECK: int * sus(int *x, _Ptr<int> y) {

int* foo() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = sus(x, y);
  *z = *z + 1;
  return z;
}
//CHECK: int* foo() {

int* bar() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = (sus(x, y));
  return z;
}
//CHECK: int* bar() {
