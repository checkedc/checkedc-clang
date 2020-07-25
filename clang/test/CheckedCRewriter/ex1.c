// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

void foo() {
  int m = 2;
  int *s = &m;
  int q[5] = { 0 };
  int *p = (int *)5;
  p = q + 3;
}
//CHECK: int q[5] = { 0 };
//CHECK-NEXT: int *p = (int *)5;
//CHECK-NEXT: p = q + 3;
