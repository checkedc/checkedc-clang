// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
#include <stdio_checked.h>

void foo(int *i) { 
  //CHECK: void foo(_Ptr<int> i) _Checked {
  *i = 3;
  printf("Test");
  //CHECK: _Unchecked { printf("Test"); };
}

