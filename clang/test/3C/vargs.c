// Tests for 3C.
//
// Checks very simple inference properties for local variables.
//
// RUN: 3c %s -- | FileCheck -match-full-lines %s
// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c %s -- | %clang_cc1  -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
#include <stdarg.h>

int doStuff(unsigned int tag, va_list arg) {
  return 0;
}
//CHECK: int doStuff(unsigned int tag, va_list arg) {

int *id(int *a) {
  return a;
}
//CHECK: _Ptr<int> id(_Ptr<int> a) {
