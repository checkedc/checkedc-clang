// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
//

void f() {
  int x[5];
  int *pa = x;
  pa += 2;
  int *p = pa;
  p = (int *)5;
}
// CHECK: int x _Checked[5];
// CHECK: _Array_ptr<int> pa =  x;

void g() {
  int *x = malloc(sizeof(int));
  int y[5];
  int **p = &x;
  int **r = 0;
  *p = y;
  (*p)[0] = 1;
  r = p;
  **r = 1;
}
// CHECK:  _Array_ptr<int> x: count(sizeof(int)) =  malloc(sizeof(int));
// CHECK:  int y _Checked[5];
// CHECK:  _Ptr<_Array_ptr<int>> p =  &x;
// CHECK:  _Ptr<_Array_ptr<int>> r =  0;
