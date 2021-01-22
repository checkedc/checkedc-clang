// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/funcptr3.checked.c -- | count 0
// RUN: rm %S/funcptr3.checked.c

void f(int *(*fp)(int *)) {
	//CHECK: void f(_Ptr<int * (int *)> fp) {
  fp(0);
}
int *g2(int *x) {
	//CHECK: int *g2(int *x) {
  return x;
}
int *g(int *x) {
	//CHECK: int *g(int *x) {
  return 0;
}
void h() {
  int *(*fp)(int *) = g;
	//CHECK: _Ptr<int * (int *)> fp =  g;
  f(g);
  f(g2);
  int *x = (int *)5;
	//CHECK: int *x = (int *)5;
  g(x);
}
