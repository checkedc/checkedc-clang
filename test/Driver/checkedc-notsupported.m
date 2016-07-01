// Checked C extension is not supported for Objective C.   Make sure driver
// rejects the flag.
//
// RUN: not %clang -fcheckedc-extension %s 2>&1 | FileCheck %s
// CHECK: error: invalid argument '-fcheckedc-extension' not allowed with 'Objective C'

extern void f() {}


