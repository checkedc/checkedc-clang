// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

int *x;
extern int *x;
void foo(void) {
  *x = 1;
}
//CHECK: _Ptr<int> x = ((void *)0);
//CHECK-NEXT: extern _Ptr<int> x;

extern int *y;
int *y;
int *bar(void) {
  y = (int*)5;
  return x;
}
//CHECK: _Ptr<int> bar(void) {

