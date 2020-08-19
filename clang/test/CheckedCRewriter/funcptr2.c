// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/funcptr2.checked.c -- | count 0
// RUN: rm %S/funcptr2.checked.c

void f(int *(*fp)(int *)) {
	//CHECK: void f(_Ptr<int * (int *)> fp) {
  int *x = (int *)5;
	//CHECK: int *x = (int *)5;
  int *z = (int *)5;
	//CHECK: int *z = (int *)5;
  z = fp(x);
}
int *g2(int *x) {
	//CHECK: int *g2(int *x) {
  return x;
}
int *g(int *x) {
	//CHECK: int *g(int *x) {
  x = (int *)5;
	//CHECK: x = (int *)5;
  return 0;
}
void h() {
  f(g);
  f(g2);
}
