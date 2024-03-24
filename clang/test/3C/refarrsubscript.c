// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/refarrsubscript.c -- | diff %t.checked/refarrsubscript.c -

int **func(int **p, int *x) {
  //CHECK_NOALL: int **func(int **p : itype(_Ptr<_Ptr<int>>), _Ptr<int> x) : itype(_Ptr<int *>) {
  //CHECK_ALL: _Array_ptr<_Ptr<int>> func(_Array_ptr<_Ptr<int>> p : count(1 + 1), _Ptr<int> x) : count(1 + 1) _Checked {
  return &(p[1]);
}

struct foo {
  int **b;
  //CHECK_NOALL: int **b;
  //CHECK_ALL: _Array_ptr<_Ptr<int>> b : count(1 + 1);
  int n;
};
int **bar(struct foo *p) {
  //CHECK_NOALL: int **bar(_Ptr<struct foo> p) : itype(_Ptr<int *>) {
  //CHECK_ALL: _Array_ptr<_Ptr<int>> bar(_Ptr<struct foo> p) : count(1 + 1) _Checked {
  int *n = &p->n;
  //CHECK: _Ptr<int> n = &p->n;
  return &(p->b[1]);
}

struct s {
  int *c;
  //CHECK: _Ptr<int> c;
};
int **getarr(struct s *q) {
  //CHECK: _Ptr<_Ptr<int>> getarr(_Ptr<struct s> q) _Checked {
  return &q->c;
}
