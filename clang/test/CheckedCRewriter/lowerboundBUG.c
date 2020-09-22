// Generates graph q0 --> ARR --> q1, to confirm that bound bound
//   constraints are properly solved
// The following test is supposed to fail with the current tool.
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
//

_Array_ptr<int> foo(_Array_ptr<int> x);
void bar() {
  int *y = 0;
  int *z = 0;
  y = foo(z);
  y[2] = 1;
}
//CHECK: _Array_ptr<int> y =  0;
//CHECK-NEXT: _Array_ptr<int> z =  0;
