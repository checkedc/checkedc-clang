// Tests for 3C.
//
// Tests for rewriting global declarations.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c  -fno-builtin -Xclang -verify -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -addcr %t.checked/global.c -- | diff %t.checked/global.c -
// expected-no-diagnostics
#include <stddef.h>
char *c;
//CHECK: char *c;
int *p, *q;
//CHECK: _Ptr<int> p = ((void *)0);
//CHECK-NEXT: int *q;
int *p1, *q1;
//CHECK: _Ptr<int> p1 = ((void *)0);
//CHECK-NEXT: _Ptr<int> q1 = ((void *)0);
int main(void) {
  q = (int *)(0xdeadbeef);
  c = (char *)(0xdeadbeef);
  return 0;
}
