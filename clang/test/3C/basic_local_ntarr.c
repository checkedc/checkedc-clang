// Tests for 3C.
//
// Tests basic rewriting of Nt_array_ptrs

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t1.unused -
//

#include <string.h>

// basic test
// just create a NT pointer
int main() {
  char *a = 0;
  char *c = 0;
  int *d = 0;
  int b;
  // this will make a as NTARR
  b = strlen(a);
  // this will make C as PTR
  c = strstr("Hello", "World");
  // this should mark d as WILD.
  d = (int *)0xdeadbeef;
  return 0;
}

//CHECK: int main() {
//CHECK-NEXT: _Nt_array_ptr<char> a =  0;
//CHECK-NEXT: _Ptr<char> c =  0;
//CHECK-NEXT: int *d = 0;
