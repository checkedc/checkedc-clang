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

struct np *sus(struct p x, struct p y) {
  struct np *z = malloc(sizeof(struct np));
  z->x = 1;
  z->x = 2;
  z += 2;
  return z;
}
//CHECK: struct np * sus(struct p x, struct p y) {

struct np *foo() {
  struct p x, y;
  x.x = 1;
  x.y = 2;
  y.x = 3;
  y.y = 4;
  struct np *z = sus(x, y);
  return z;
}
//CHECK: struct np *foo() {

struct np *bar() {
  struct p x, y;
  x.x = 1;
  x.y = 2;
  y.x = 3;
  y.y = 4;
  struct np *z = sus(x, y);
  z += 2;
  return z;
}
//struct np *bar() {
