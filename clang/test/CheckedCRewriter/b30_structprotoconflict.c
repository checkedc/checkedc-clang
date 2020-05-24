// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

struct np {
  int x;
  int y;
};

struct p {
  int *x;
  char *y;
};
//CHECK: _Ptr<int> x;
//CHECK-NEXT: _Ptr<char> y;

struct r {
  int data;
  struct r *next;
};

struct r *sus(struct r *, struct r *);
//CHECK: struct r *sus(_Ptr<struct r> x, _Ptr<struct r> y) : itype(_Ptr<struct r>);
// FIXME-- the above annotation is wrong; the cast at bar() is making it seem like it should be an itype

struct np *foo() {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct np *z = (struct np *) sus(&x, &y);
  return z;
}

struct r *bar() {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct r *z = (struct r *) sus(&x, &y);
  return z;
}
//CHECK: _Ptr<struct r> bar(void) {
//CHECK: _Ptr<struct r> z =  (struct r *) sus(&x, &y);

struct r *sus(struct r *x, struct r *y) {
  x->next += 1;
  struct r *z = malloc(sizeof(struct r));
  z->data = 1;
  z->next = 0;
  return z;
}
//CHECK: struct r *sus(_Ptr<struct r> x, _Ptr<struct r> y) : itype(_Ptr<struct r>) {
//CHECK: _Ptr<struct r> z =  malloc(sizeof(struct r));
