// Checked C extension is not supported for C++.   Make sure driver
// warns about the -fchecked-extension and ignores it.
//
// RUN: %clang -c -fcheckedc-extension %s 2>&1 | FileCheck %s
// CHECK: warning: Checked C extension not supported with 'C++'; ignoring '-fcheckedc-extension'
//
// Have clang compile this file as a C file.
// RUN: %clang -c -fcheckedc-extension -x c %s
//
// Have clang-cl compile this file as a C file.
// RUN: %clang_cl -c -Xclang -fcheckedc-extension /TC %s

extern void f() {}


