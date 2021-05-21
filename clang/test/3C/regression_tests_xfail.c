// Tests for 3C.
//
// Tests 3c tool for any regressions.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o %t.unused -
//
// XFAIL: *

#include <stdlib.h>

unsigned char *func(void) {
  char *ptr = NULL;
  return (unsigned char *)ptr;
}
//CHECK: _Nt_array_ptr<unsigned char> func(void) {
//CHECK-NEXT: _Nt_array_ptr<char> ptr =  NULL;

int main() {

  char *ptr1 = NULL;
  char *d = "sss";

  ptr1 = (char *)calloc(1, sizeof(char));
  d = func();
  return 0;
}
//CHECK: _Ptr<char> ptr1 =  NULL;
//CHECK-NEXT: _Nt_array_ptr<char> d: count(3) =  "sss";
