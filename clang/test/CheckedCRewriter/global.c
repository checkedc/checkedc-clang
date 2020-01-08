// Tests for Checked C rewriter tool.
//
// Tests for rewriting global declarations.
//
// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
// RUN: checked-c-convert %s -- | %clang_cc1 -fno-builtin -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
#define NULL ((void*)0)
char *c;
//CHECK: char *c;
int *p,*q;
//CHECK: _Ptr<int> p = NULL;
//CHECK-NEXT: int *q;
int *p1, *q1;
//CHECK: _Ptr<int> p1 = NULL;
//CHECK-NEXT: _Ptr<int> q1 = NULL;
int main(void) { 
  q = (int*)(0xdeadbeef);
  c = (char*)(0xdeadbeef);
  return 0;
}
