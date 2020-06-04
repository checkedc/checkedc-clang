// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

int f(int *p);
void bar() {
  int (*fp)(int *p) = f;
  f((void*)0);
}
// CHECK:   _Ptr<int (int *)> fp =  f;

int mul_by_2(int x) { 
    return x * 2;
}

int (*foo(void)) (int) {
    return mul_by_2;
} 
// CHECK: _Ptr<int (int )> foo(void) {

