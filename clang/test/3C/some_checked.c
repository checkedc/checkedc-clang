// Tests for the Checked C rewriter tool.
//
// RUN: 3c %s -- -fcheckedc-extension | FileCheck -match-full-lines %s
// RUN: 3c %s -- -fcheckedc-extension | %clang_cc1  -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
//

void do_something(int *a, int b) {
  *a = b;
}
//CHECK: void do_something(_Ptr<int> a, int b) {

void test(_Array_ptr<int> p : count(len), int len) {
  _Array_ptr<int> r : count(len - 1) =
    _Dynamic_bounds_cast<_Array_ptr<int>>(p + 1, count(len - 1));
}

