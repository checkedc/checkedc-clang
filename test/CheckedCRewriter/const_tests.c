// Tests for Checked C rewriter tool.
//
// Checks properties of functions.
//
// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
// RUN: checked-c-convert %s -- | %clang_cc1 -verify -fcheckedc-extension -x c -
// expected-no-diagnostics

void cst1(const int *a) {
  int b = *a;
}
//CHECK: void cst1(_Ptr<const int>  a) {

void cst2(int * const a) {
  *a = 0;
}
//CHECK: void cst2(const _Ptr<int>  a) {

void cst3(const int *a, int i) {
  int c = *(a+i);
}
//CHECK: void cst3(const int *a, int i) {
//CHECK-NEXT: int c = *(a+i);

void cst4(const int *b) {
  int c = *b;
  const int *d = b;
  int e = *d;
}
//CHECK: void cst4(_Ptr<const int>  b) {
//CHECK-NEXT: int c = *b;
//CHECK-NEXT: _Ptr<const int>  d;
//CHECK-NEXT: int e = *d;
