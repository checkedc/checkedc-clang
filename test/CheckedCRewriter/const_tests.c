// Tests for Checked C rewriter tool.
//
// Checks properties of functions.
//
// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
// RUN: checked-c-convert %s -- | %clang_cc1 -verify -fcheckedc-extension -x c -
// expected-no-diagnostics

// Have something so that we always get some output.
void a0(void) {
  int q = 0;
  int *k = &q;
  *k = 0;
}
//CHECK: int q = 0;
//CHECK-NEXT: _Ptr<int> k = &q;
//CHECK-NEXT: *k = 0;
//CHECK-NEXT: }

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
