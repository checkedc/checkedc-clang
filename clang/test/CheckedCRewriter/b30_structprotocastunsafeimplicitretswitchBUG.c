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

struct np *sus(struct r *, struct r *);
//CHECK: struct np *sus(struct r *x : itype(_Ptr<struct r>), struct r *y : itype(_Ptr<struct r>)) : itype(_Ptr<struct np>);

struct np *foo() {
  struct r *x, *y;
  x->data = 2;
  y->data = 1;
  x->next = &y;
  y->next = &x;
  struct np *z = (struct r *) sus(x, y);
  return z;
}
//CHECK: struct np *foo() {

struct r *bar() {
  struct r *x, *y;
  x->data = 2;
  y->data = 1;
  x->next = &y;
  y->next = &x;
  struct r *z = sus(x, y);
  return z;
}
//CHECK: _Ptr<struct r> bar(void) {

struct np *sus(struct r *x, struct r *y) {
  x->next += 1;
  struct np *z = malloc(sizeof(struct np));
  z->x = 1;
  z->y = 0;
  return z;
}
//CHECK: struct np *sus(struct r *x : itype(_Ptr<struct r>), struct r *y : itype(_Ptr<struct r>)) : itype(_Ptr<struct np>) {
