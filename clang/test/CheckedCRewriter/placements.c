// Tests for Checked C rewriter tool.
//
// Checks properties of functions.
//
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK","CHECK_NOALL","CHECK-NEXT" %s
// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK","CHECK_ALL","CHECK-NEXT" %s
// RUN: cconv-standalone -addcr %s -- | %clang_cc1  -verify -fcheckedc-extension -x c -
// RUN: cconv-standalone -addcr -alltypes -output-postfix=checked %s 
// RUN: cconv-standalone -addcr -alltypes %S/placements.checked.c -- | count 0
// RUN: rm %S/placements.checked.c
// expected-no-diagnostics
void what(const char *s, int q); 
//CHECK_NOALL: void what(const char *s, int q);
//CHECK_ALL: void what(_Array_ptr<const char> s : count(q), int q);

void what(const char *s, int q) {
  char v = s[5];
}
//CHECK_NOALL: void what(const char *s, int q) {
//CHECK_ALL: void what(_Array_ptr<const char> s : count(q), int q) _Checked {

void foo(_Ptr<int> a) {
  *a = 0;
}
//CHECK: void foo(_Ptr<int> a) _Checked {

void foo2(_Ptr<int> a) {
  _Ptr<int> b = a;
  *b = 0;
}
//CHECK: void foo2(_Ptr<int> a) _Checked { 
//CHECK-NEXT: _Ptr<int> b = a;

void bar(int *a : itype(_Ptr<int>) ) {
  *a = 0;
}
//CHECK: void bar(int *a : itype(_Ptr<int>) ) _Checked {

extern int* baz(void) : itype(_Ptr<int>);
//CHECK: extern int*  baz(void) : itype(_Ptr<int>);

// force output
int *p;
