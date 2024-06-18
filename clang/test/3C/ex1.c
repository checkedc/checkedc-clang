// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/ex1.c -- | diff %t.checked/ex1.c -

void foo() {
  int m = 2;
  int *s = &m;
  //CHECK: _Ptr<int> s = &m;
  int q[5] = {0};
  //CHECK: int q[5] = {0};
  int *p = (int *)5;
  //CHECK: int *p = (int *)5;
  p = q + 3;
}
