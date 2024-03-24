// Tests for the 3C.
//
// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S %s -- | %clang -c  -Xclang -verify -fcheckedc-extension -x c -o /dev/null -
// expected-no-diagnostics
//

void do_something(int *a, int b) { *a = b; }
//CHECK: void do_something(_Ptr<int> a, int b) { *a = b; }

void test(_Array_ptr<int> p : count(len), int len) {
  _Array_ptr<int> r
      : count(len - 1) =
            _Dynamic_bounds_cast<_Array_ptr<int>>(p + 1, count(len - 1));
}
