// RUN: 3c -addcr %s | FileCheck -match-full-lines --check-prefixes="CHECK" %s

// Currently not running clang on the output,
// since it cannot find "dummy.h", even with -L%S

#include "dummy.h"
// Dummy to cause output
void f(int *x) {}
//CHECK: void f(_Ptr<int> x) _Checked {}

void foo(char c) { 
  //CHECK: void foo(char c) {
  unsafe(&c);
}

