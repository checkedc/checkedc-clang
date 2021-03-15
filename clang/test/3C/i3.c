// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/i3.c -- | diff %t.checked/i3.c -

static int *f(int *x) {
  //CHECK: static int *f(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  x = (int *)5;
  //CHECK: x = (int *)5;
  return x;
}
/* force output */
int *p;
//CHECK: _Ptr<int> p = ((void *)0);
