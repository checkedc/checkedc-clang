// Tests for Checked C rewriter tool.
//
// Checks very simple inference properties for local variables.
//
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone %s -- | %clang_cc1  -verify -fcheckedc-extension -x c -
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
