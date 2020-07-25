// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

void f(int *(*fp)(int *)) {
  fp(0);
}
//CHECK: void f(_Ptr<_Ptr<int> (_Ptr<int> )> fp) {
int *g2(int *x) {
  return x;
}
//CHECK: _Ptr<int> g2(_Ptr<int> x) {
int *g(int *x) {
  return 0;
}
//CHECK: _Ptr<int> g(_Ptr<int> x) {
void h() {
  int *(*fp)(int *) = g;
  f(g);
  f(g2);
  g(0);
}
//CHECK: _Ptr<_Ptr<int> (_Ptr<int> )> fp =  g;
