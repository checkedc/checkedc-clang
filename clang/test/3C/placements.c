// Tests for 3C.
//
// Checks properties of functions.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK","CHECK_NOALL","CHECK-NEXT" %s
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK","CHECK_ALL","CHECK-NEXT" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c  -Xclang -verify -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -addcr -alltypes %t.checked/placements.c -- | diff %t.checked/placements.c -
// expected-no-diagnostics
void what(const char *s, int q);
//CHECK_NOALL: void what(const char *s : itype(_Ptr<const char>), int q);
//CHECK_ALL: void what(_Array_ptr<const char> s : count(5 + 1), int q);

void what(const char *s, int q) { char v = s[5]; }
//CHECK_NOALL: void what(const char *s : itype(_Ptr<const char>), int q) { char v = s[5]; }
//CHECK_ALL: void what(_Array_ptr<const char> s : count(5 + 1), int q) _Checked { char v = s[5]; }

void foo(_Ptr<int> a) { *a = 0; }
//CHECK: void foo(_Ptr<int> a) _Checked { *a = 0; }

void foo2(_Ptr<int> a) {
  _Ptr<int> b = a;
  *b = 0;
}
//CHECK: void foo2(_Ptr<int> a) _Checked {
//CHECK-NEXT: _Ptr<int> b = a;

void bar(int *a : itype(_Ptr<int>)) { *a = 0; }
//CHECK: void bar(_Ptr<int> a) _Checked { *a = 0; }

extern int *baz(void) : itype(_Ptr<int>);
//CHECK: extern int *baz(void) : itype(_Ptr<int>);

// force output
int *p;
