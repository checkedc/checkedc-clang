// Tests for clang-specific issues with the _Dynamic_check keyword
//
// RUN: %clang_cc1 -fcheckedc-extension -verify -ast-dump %s | FileCheck %s

#include <stdbool.h>

void f1(_Ptr<void> i) {
  _Dynamic_check(i != 0);
  // CHECK: CallExpr
  // CHECK: DeclRefExpr
  // CHECK-SAME: <builtin fn type>
  // CHECK-SAME: _Dynamic_check
}


void f2(void) {
  _Dynamic_check(true, "Message"); // expected-error {{too many arguments to function call, expected 1}}
}
