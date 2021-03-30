// Tests for the 3C.
//
// When multiple constants are potential bounds, pick the lower constant.
// Issue: https://github.com/correctcomputation/checkedc-clang/issues/390
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -fcheckedc-extension -Xclang -verify -x c -o %t1.unused -
// expected-no-diagnostics
//
int a(char * : itype(_Nt_array_ptr<char>));
//CHECK: int a(char * : itype(_Nt_array_ptr<char>));
void b(char *p) { a(p); }
//CHECK: void b(_Nt_array_ptr<char> p : count(4)) { a(p); }
void c(void) {
  char foo[256];
  b(foo);
  b("test");
}
//CHECK: char foo _Nt_checked[256];
