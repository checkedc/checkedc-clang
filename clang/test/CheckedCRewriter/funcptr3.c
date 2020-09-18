// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

void f(int *(*fp)(int *)) {
  fp(0);
}
//CHECK: void f(_Ptr<int* (int *)> fp) {
int *g2(int *x) {
  return x;
}
int *g(int *x) {
  return 0;
}
void h() {
  int *(*fp)(int *) = g;
  f(g);
  f(g2);
  int *x = (int *)5;
  g(x);
}
