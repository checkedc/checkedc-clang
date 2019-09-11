// Tests for Checked C rewriter tool.
//
// Tests basic rewriting of Nt_array_ptrs

// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
//

#include <string_checked.h>
// basic test
// just create a NT pointer
int main() {
  char *a = 0;
  char *c = 0;
  int *d = 0;
  int b;
  // this will make a as NTARR
  b = strlen(a);
  // this will make C as NTArr
  c = strstr("Hello", "World");
  // this should mark d as WILD.
  d = (int*)0xdeadbeef;
  return 0;
}

//CHECK: int main() {
//CHECK-NEXT: _Nt_array_ptr<char> a =  0;
//CHECK-NEXT: _Nt_array_ptr<char> c =  0;
//CHECK-NEXT: int *d = 0;
