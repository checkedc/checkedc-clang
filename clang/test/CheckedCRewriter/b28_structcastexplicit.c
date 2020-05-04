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

struct r *sus(struct r x, struct r y) {
  x.next += 1;
  struct r *z = malloc(sizeof(struct r));
  z->data = 1;
  z->next = NULL;
  return z;
}
//CHECK: struct r *sus(struct r x, struct r y) : itype(_Ptr<struct r>) {

struct r *foo() {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct r z = (struct r *) sus(x, y);
  return z;
}
//CHECK: _Ptr<struct r> foo(void) {

struct np *bar() {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct np *z = (struct np *) sus(x, y);
  return z;
}
//CHECK: struct np *bar() {
//CHECK: struct np *z = (struct np *) sus(x, y);
