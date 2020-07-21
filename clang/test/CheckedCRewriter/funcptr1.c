// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

void f(int *(*fp)(int *)) {
  int *x = (int *)5;
  int *z = (int *)5;
  z = fp(x);
}
//CHECK: void f(_Ptr<int* (int *)> fp) {

int *g(int *x) {
  x = (int *)5;
  return 0;
}
void h() {
  f(g);
}
