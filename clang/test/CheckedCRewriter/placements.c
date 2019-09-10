// Tests for Checked C rewriter tool.
//
// Checks properties of functions.
//
// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
// RUN: checked-c-convert %s -- | %clang_cc1 -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
void what(const char *s, int q); 
//CHECK: void what(const char *s, int q);

void what(const char *s, int q) {
  char v = s[5];
}
//CHECK: void what(const char *s, int q) {

void foo(_Ptr<int> a) {
  *a = 0;
}
//CHECK: void foo(_Ptr<int> a) {

void foo2(_Ptr<int> a) {
  _Ptr<int> b = a;
  *b = 0;
}
//CHECK: void foo2(_Ptr<int> a) { 
//CHECK-NEXT: _Ptr<int> b = a;

void bar(int *a : itype(_Ptr<int>) ) {
  *a = 0;
}
//CHECK: void bar(int* a : itype(_Ptr<int>)) {

extern int* baz(void) : itype(_Ptr<int>);
//CHECK: extern int*  baz(void) : itype(_Ptr<int>);
