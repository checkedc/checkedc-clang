// Checked C extension is not supported for CUDA.   Make sure driver
// rejects the flag.
//
// RUN: not %clang -fcheckedc-extension -nocudalib -nocudainc %s 2>&1 | FileCheck %s
// CHECK: error: invalid argument '-fcheckedc-extension' not allowed with 'CUDA'
//
// Have clang compile this file as a C file.
// RUN: %clang -c -fcheckedc-extension -x c %s
//
// Have clang-cl compile this file as a C file.
// RUN: %clang_cl -c -Xclang -fcheckedc-extension /TC %s

extern void f() {}


