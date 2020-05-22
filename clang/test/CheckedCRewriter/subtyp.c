// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
//

void take(int *p : itype(_Nt_array_ptr<int>));
int *foo(int *x) {
  take(x);
  return x;
}
// CHECK: _Nt_array_ptr<int> foo(int *x : itype(_Nt_array_ptr<int>)) {
void bar() {
  int *x = 0;
  foo(x);
}
// CHECK: _Nt_array_ptr<int> x =  0;
void baz() {
  int *x = (int *)5;
  foo(x);
}
