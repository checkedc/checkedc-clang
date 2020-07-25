// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

int **func(int **p, int *x) {
  return &(p[1]);
} 
//CHECK_NOALL: int ** func(int **p, _Ptr<int> x) {
//CHECK_ALL: _Array_ptr<_Ptr<int>> func(_Array_ptr<_Ptr<int>> p, _Ptr<int> x) {

struct foo { int **b; int n; };
int **bar(struct foo *p) {
  int *n = &p->n;
  return &(p->b[1]);
}
//CHECK_NOALL: struct foo { int **b; int n; };
//CHECK_NOALL-NEXT: int ** bar(_Ptr<struct foo> p) {
//CHECK_NOALL-NEXT:  _Ptr<int> n =  &p->n;
//CHECK_ALL: struct foo { _Array_ptr<_Ptr<int>> b; int n; };
//CHECK_ALL-NEXT: _Array_ptr<_Ptr<int>> bar(_Ptr<struct foo> p) {
//CHECK_ALL-NEXT:  _Ptr<int> n =  &p->n;

struct s { int *c; };
int **getarr(struct s *q) {
  return &q->c;
}
//CHECK_NOALL: struct s { _Ptr<int> c; };
//CHECK_NOALL-NEXT:_Ptr<_Ptr<int>> getarr(_Ptr<struct s> q) {
//CHECK_ALL: struct s { _Ptr<int> c; };
//CHECK_ALL-NEXT:_Ptr<_Ptr<int>> getarr(_Ptr<struct s> q) {

