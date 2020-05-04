// RUN: CConvertStandalone %s -- | FileCheck -match-full-lines %s

char *sus(int *x, int*y) {
  char *z = malloc(sizeof(char));
  *z = 1;
  x++;
  *x = 2;
  return z;
}
//CHECK: char *sus(int *x, _Ptr<int> y) : itype(_Ptr<char>) {

char* foo() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  char *z = (int *) sus(x, y);
  *z = *z + 1;
  return z;
}
//CHECK: char* foo() {

int* bar() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = sus(x, y);
  return z;
}
//CHECK: _Ptr<int> bar(void) {
