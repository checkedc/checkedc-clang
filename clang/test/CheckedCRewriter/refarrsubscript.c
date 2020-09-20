// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/refarrsubscript.checked.c -- | count 0
// RUN: rm %S/refarrsubscript.checked.c

int **func(int **p, int *x) {
	//CHECK_NOALL: int **func(int **p, _Ptr<int> x) {
	//CHECK_ALL: _Array_ptr<_Ptr<int>> func(_Array_ptr<_Ptr<int>> p, _Ptr<int> x) _Checked {
  return &(p[1]);
} 

struct foo { int **b; int n; };
	//CHECK_NOALL: struct foo { int **b; int n; };
	//CHECK_ALL: struct foo { _Array_ptr<_Ptr<int>> b; int n;   };
int **bar(struct foo *p) {
	//CHECK_NOALL: int **bar(_Ptr<struct foo> p) {
	//CHECK_ALL: _Array_ptr<_Ptr<int>> bar(_Ptr<struct foo> p) _Checked {
  int *n = &p->n;
	//CHECK: _Ptr<int> n =  &p->n;
  return &(p->b[1]);
}

struct s { int *c; };
	//CHECK: struct s { _Ptr<int> c; };
int **getarr(struct s *q) {
	//CHECK: _Ptr<_Ptr<int>> getarr(_Ptr<struct s> q) _Checked {
  return &q->c;
}

