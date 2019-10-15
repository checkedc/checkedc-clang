// Tests for Checked C rewriter tool.
//
// Tests rewriting of Nt_array_ptrs within structure fields

// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
//

#include <string_checked.h>
// This tests the propagation of constraints
// within the fields of structure.
typedef struct {
  int *ptr;
  char *ntptr;
} foo;
//CHECK: typedef struct {
//CHECK-NEXT: _Ptr<int> ptr;
//CHECK-NEXT: _Nt_array_ptr<char> ntptr;

foo obj1 = {};

int* func(int *ptr, char *ntptr) {
  obj1.ptr = ptr;
  obj1.ntptr = strstr(ntptr, "world");
  return ptr;
}
//CHECK: _Ptr<int> func(_Ptr<int> ptr, _Nt_array_ptr<char> ntptr) {

int main() {
  int a;
  int *b = 0;
  char *wil = 0;
  a = strlen(wil);
  b = func(&a, wil);
  return 0;
}
//CHECK: int main() {
//CHECK-NEXT: int a;
//CHECK-NEXT: _Ptr<int> b =  0;
//CHECK-NEXT: _Nt_array_ptr<char> wil =  0;
