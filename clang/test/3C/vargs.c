// Tests for 3C.
//
// Checks very simple inference properties for local variables.
//
// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S %s -- | %clang -c  -Xclang -verify -fcheckedc-extension -x c -o /dev/null -
// expected-no-diagnostics
#include <stdarg.h>

int doStuff(unsigned int tag, va_list arg) { return 0; }
//CHECK: int doStuff(unsigned int tag, va_list arg) { return 0; }

int *id(int *a) { return a; }
//CHECK: _Ptr<int> id(_Ptr<int> a) { return a; }
