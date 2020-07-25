// Tests for Checked C rewriter tool.
//
// Tests for rewriting global declarations.
//
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
<<<<<<< HEAD
// RUN: cconv-standalone %s -- | %clang_cc1 -fno-builtin -verify -fcheckedc-extension -x c -
=======
// RUN: cconv-standalone %s -- | %clang_cc1 -fignore-checkedc-pointers -fno-builtin -verify -fcheckedc-extension -x c -
>>>>>>> origin/BigRefactor
// expected-no-diagnostics
#define NULL ((void*)0)
char *c;
//CHECK: char *c;
int *p,*q;
//CHECK: _Ptr<int> p = ((void *)0);
//CHECK-NEXT: int *q;
int *p1, *q1;
//CHECK: _Ptr<int> p1 = ((void *)0);
//CHECK-NEXT: _Ptr<int> q1 = ((void *)0);
int main(void) { 
  q = (int*)(0xdeadbeef);
  c = (char*)(0xdeadbeef);
  return 0;
}
