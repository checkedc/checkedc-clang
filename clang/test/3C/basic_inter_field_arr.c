// Tests for Checked C rewriter tool.
//
// Tests properties about arr constraints propagation.
//
// RUN: 3c %s -- | FileCheck -match-full-lines %s
// RUN: 3c %s -- | %clang_cc1  -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
//
// This tests the propagation of constraints
// within the fields of structure.
typedef struct {
  int *ptr;
  char *arrptr;
} foo;
//CHECK: typedef struct {
//CHECK-NEXT: _Ptr<int> ptr;
//CHECK: char *arrptr;

foo obj1 = {};

int* func(int *ptr, char *arrptr) {
  obj1.ptr = ptr;
  arrptr++;
  obj1.arrptr = arrptr;
  return ptr;
}
//CHECK: _Ptr<int> func(_Ptr<int> ptr, char *arrptr) {

int main() {
  int a;
  int *b = 0;
  char *wil = 0;
  wil = (char*)0xdeadbeef;
  b = func(&a, wil);
  return 0;
}
//CHECK: int main() {
//CHECK-NEXT: int a;
//CHECK-NEXT: _Ptr<int> b =  0;
//CHECK-NEXT: char *wil = 0;
