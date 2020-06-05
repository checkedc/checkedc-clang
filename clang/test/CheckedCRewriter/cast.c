// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

int foo(int* p) {
  *p = 5;
  int x = (int)p; // cast is safe
  return x;
}
//CHECK: int foo(_Ptr<int> p) {

void bar(void) {
  int a = 0;
  int *b = &a;
  char *c = (char *)b;
  int *d = (int *)5;
//  int *e = (int *)(a+5);
}
//CHECK:   int *b = &a;
//CHECK-NEXT: char *c = (char *)b;
//CHECK-NEXT: int *d = (int *)5;
