// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/funcptr4.checked.c -- | count 0
// RUN: rm %S/funcptr4.checked.c

void f(int *(*fp)(int *)) {
	//CHECK: void f(_Ptr<_Ptr<int> (_Ptr<int> )> fp) _Checked {
  fp(0);
}
int *g2(int *x) {
	//CHECK: _Ptr<int> g2(_Ptr<int> x) _Checked {
  return x;
}
int *g(int *x) {
	//CHECK: _Ptr<int> g(_Ptr<int> x) _Checked {
  return 0;
}
void h() {
  int *(*fp)(int *) = g;
	//CHECK: _Ptr<_Ptr<int> (_Ptr<int> )> fp =  g;
  f(g);
  f(g2);
  g(0);
}
