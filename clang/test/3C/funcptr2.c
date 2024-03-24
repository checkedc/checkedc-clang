// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/funcptr2.c -- | diff %t.checked/funcptr2.c -

void f(int *(*fp)(int *)) {
  //CHECK: void f(_Ptr<int *(int * : itype(_Ptr<int>)) : itype(_Ptr<int>)> fp) {
  int *x = (int *)5;
  //CHECK: int *x = (int *)5;
  int *z = (int *)5;
  //CHECK: int *z = (int *)5;
  z = fp(x); /* GENERATE CHECK */
  //CHECK: z = fp(x); /* GENERATE CHECK */
}
int *g2(int *x) {
  //CHECK: int *g2(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  return x;
}
int *g(int *x) {
  //CHECK: int *g(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  x = (int *)5;
  //CHECK: x = (int *)5;
  return 0;
}
void h() {
  //CHECK: void h() _Checked {
  f(g);
  f(g2);
}
