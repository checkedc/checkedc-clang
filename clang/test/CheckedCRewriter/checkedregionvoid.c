// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
#include "dummy.h"
// Dummy to cause output
void f(int *x) {}

void foo(char c) { 
  //CHECK: void foo(char c) {
  unsafe(&c);
}

