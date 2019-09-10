// Checked C extension is supported for C.   Make sure driver
// accepts the flag for C and rejects it when the file is
// compiled as another language.
//
// RUN: %clang -c -fcheckedc-extension -o %t %s
// RUN: %clang_cl -c -Xclang -fcheckedc-extension -o %t -- %s
//
// Have clang compile this file as C++ file.
// RUN: not %clang -c -fcheckedc-extension -x c++ -o %t %s 2>&1 \
// RUN:  | FileCheck %s -check-prefix=check-cpp
// check-cpp: warning: Checked C extension not supported with 'C++'; ignoring '-fcheckedc-extension'
//
// Have clang-cl compile this file as a C++ file.
// RUN: not %clang_cl -c -Xclang -fcheckedc-extension /TP /Fo%t -- %s 2>&1 \
// RUN:  | FileCheck %s -check-prefix=clcheck-cpp
// clcheck-cpp: warning: Checked C extension not supported with 'C++'; ignoring '-fcheckedc-extension'
//
// RUN: not %clang -c -fcheckedc-extension -x cuda -nocudalib -nocudainc %s -o %t 2>&1 \
// RUN:  | FileCheck %s -check-prefix=check-cuda
// check-cuda: warning: Checked C extension not supported with 'CUDA'; ignoring '-fcheckedc-extension'
//
// RUN: not %clang -c -fcheckedc-extension -x cl %s -o %t 2>&1 \
// RUN:  | FileCheck %s -check-prefix=check-opencl
// check-opencl: warning: Checked C extension not supported with 'OpenCL'; ignoring '-fcheckedc-extension'
//
// RUN: not %clang -c -fcheckedc-extension -x objective-c %s -o %t 2>&1 \
// RUN:  | FileCheck %s -check-prefix=check-objc
// check-objc: warning: Checked C extension not supported with 'Objective C'; ignoring '-fcheckedc-extension'
//
// RUN: not %clang -c -fcheckedc-extension -x objective-c++ %s -o %t 2>&1 \
// RUN:  | FileCheck %s -check-prefix=check-objcpp
// check-objcpp: warning: Checked C extension not supported with 'Objective C/C++'; ignoring '-fcheckedc-extension'

extern void f(_Ptr<int> p) {}
