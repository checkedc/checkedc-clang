// RUN: CConvertStandalone %s -- | FileCheck -match-full-lines %s

int *sus(int *, int *);
//CHECK: _Ptr<int> sus(int *x, _Ptr<int> y);

int* foo() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = (int *) sus(x, y);
  *z = *z + 1;
  return z;
}
//CHECK: _Ptr<int> foo(void) {

char* bar() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  char *z = (sus(x, y));
  return z;
}
//CHECK: _Ptr<char> bar(void) {

int *sus(int *x, int*y) {
  int *z = malloc(sizeof(int));
  *z = 1;
  x++;
  *x = 2;
  return z;
}
//CHECK: _Ptr<int> sus(int *x, _Ptr<int> y) {
