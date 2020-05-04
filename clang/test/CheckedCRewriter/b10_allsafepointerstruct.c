// RUN: CConvertStandalone %s -- | FileCheck -match-full-lines %s

struct np {
  int x;
  int y;
};

struct p {
  int *x;
  char *y;
};

struct r {
  int data;
  struct r *next;
};

struct p *sus(struct p *x, struct p *y) {
  x->y += 1;
  struct p *z = malloc(sizeof(struct p));
  return z;
}
//CHECK: _Ptr<struct p> sus(_Ptr<struct p> x, _Ptr<struct p> y) {

struct p *foo() {
  int ex1 = 2, ex2 = 3;
  struct p *x, *y;
  x->x = &ex1;
  y->x = &ex2;
  x->y = &ex2;
  y->y = &ex1;
  struct p *z = (struct p *) sus(x, y);
  return z;
}
//CHECK: _Ptr<struct p> foo(void) {

struct p *bar() {
  int ex1 = 2, ex2 = 3;
  struct p *x, *y;
  x->x = &ex1;
  y->x = &ex2;
  x->y = &ex2;
  y->y = &ex1;
  struct p *z = (struct p *) sus(x, y);
  return z;
}
//CHECK: _Ptr<struct p> bar(void) {
