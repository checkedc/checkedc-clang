// Tests for Code Generation with Checked C Extension
// This makes sure we're generating something sensible for _Dynamic_check
// invocations.
//
// RUN: %clang_cc1 -fcheckedc-extension -verify -emit-llvm %s -o %t

void f0(void) {
  _Dynamic_check(1);
}

// Function with single dynamic check
void f1(void) {
  _Dynamic_check(0); // expected-warning {{dynamic check will always fail}}
}

void f2(void) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcheckedc"

  _Dynamic_check(0);

#pragma clang diagnostic pop
}