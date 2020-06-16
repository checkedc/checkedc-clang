// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s

int **func(int **p, int *x) {
  return &(p[1]);
}
//CHECK: _Array_ptr<_Ptr<int>> func(_Array_ptr<_Ptr<int>> p, _Ptr<int> x) {

struct foo { int **b; int n; };
int **bar(struct foo *p) {
  int *n = &p->n;
  return &(p->b[1]);
}
//CHECK: struct foo { _Nt_array_ptr<_Ptr<int>> b; int n; };
//CHECK-NEXT: _Nt_array_ptr<_Ptr<int>> bar(_Ptr<struct foo> p) {
//CHECK-NEXT:  _Ptr<int> n =  &p->n;

struct s { int *c; };
int **getarr(struct s *q) {
  return &q->c;
}
//CHECK: struct s { _Ptr<int> c; };
//CHECK-NEXT:_Ptr<_Ptr<int>> getarr(_Ptr<struct s> q) {

