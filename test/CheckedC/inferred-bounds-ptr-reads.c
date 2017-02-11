// Tests of inferred bounds for expressions in assignments and declarations.
// The goal is to check that the bounds are being inferred correctly.  This
// file covers:
// - Reads of memory through checked pointers, including checked arrays.
//
// The tests have the general form:
// 1. Some C code.
// 2. A description of the inferred bounds for that C code:
//  a. The expression
//  b. The inferred bounds.
// The description uses AST dumps.
//
// This line is for the clang test infrastructure:
// RUN: %clang_cc1 -fcheckedc-extension -verify -fdump-inferred-bounds %s | FileCheck %s
// expected-no-diagnostics

//-------------------------------------------------------------------------//
// Test assignment of integers to _Array_ptr variables.  This covers both  //
// 0 (NULL) and non-zero integers (the results of casts).                  //
//-------------------------------------------------------------------------//

int f1(_Array_ptr<int> a : bounds(a, a + 5)) {
  int x = *a;
  int y = a[3];
  return x + y;
}

int f2(void) {
  int arr _Checked[6] = { 0, 1, 2, 3, 4 };
  int x = *arr;
  int y = arr[2];
  return x + y;
}

int f3(int b _Checked[7]) {
  int x = *b;
  int y = b[3];
  return x + y;
}

