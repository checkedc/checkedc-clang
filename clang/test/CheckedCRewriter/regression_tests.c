// Tests for Checked C rewriter tool.
//
// Tests checked-c-convert tool for any regressions.
//
// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
//

#include <stdlib_checked.h>

unsigned char *func(void) {
   char *ptr = NULL;
   return (unsigned char*)ptr;
}
//CHECK: _Nt_array_ptr<unsigned char> func(void) {
//CHECK-NEXT: _Nt_array_ptr<char> ptr =  NULL;

int main() {

  char *ptr1 = NULL;
  char *d = "sss";

  ptr1 = (char *) calloc(1, sizeof(char));
  d = func();
  return 0;
}
//CHECK: _Ptr<char> ptr1 =  NULL;
//CHECK-NEXT: _Nt_array_ptr<char> d =  "sss";
