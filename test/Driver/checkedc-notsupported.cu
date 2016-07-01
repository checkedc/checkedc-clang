// Checked C extension is not supported for CUDA.   Make sure driver
// rejects the flag.
//
// RUN: not %clang -fcheckedc-extension %s 2>&1 | FileCheck %s
// CHECK: error: invalid argument '-fcheckedc-extension' not allowed with 'CUDA'

extern void f() {}


