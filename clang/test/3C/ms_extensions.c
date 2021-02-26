// Regression test for implicit injection of `struct _GUID` on Windows with an
// invalid source location crashing DeclRewriter::detectInlineStruct.
// (https://github.com/correctcomputation/checkedc-clang/issues/448)

// RUN: 3c -base-dir=%S %s -- -fms-extensions | FileCheck -match-full-lines %s

int *x;
// CHECK: _Ptr<int> x = ((void *)0);
