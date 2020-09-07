// Tests for Checked C rewriter tool.
//
// Tests cconv-standalone tool for any regressions.
//
// The following test is supposed to fail with the current tool.
// XFAIL: *
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
// 
// XFAIL: *

#include <stddef.h>

_Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);

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
//CHECK-NEXT: _Nt_array_ptr<char> d: count(3) =  "sss";
