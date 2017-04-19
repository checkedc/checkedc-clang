// Tests for Checked C rewriter tool.
//
// Tests for malloc and friends. 
//
// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
// RUN: checked-c-convert %s -- | %clang_cc1 -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
//
extern void *malloc(unsigned long);

void dosomething(void) {
  int a = 0;
  int *b = &a;
  *b = 1;
  return;
}

void foo(void) {
  int *a = (int *) malloc(sizeof(int));
  *a = 0;
  return;
}
//CHECK: void foo(void) {
//CHECK-NEXT: _Ptr<int> a = (int *) malloc(sizeof(int));
