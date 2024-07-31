// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/b21_calleepointerstructproto.c -- -Wno-error=int-conversion | diff %t.checked/b21_calleepointerstructproto.c -
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct np {
  int x;
  int y;
};

struct p {
  int *x;
  //CHECK: int *x;
  char *y;
  //CHECK: char *y;
};

struct r {
  int data;
  struct r *next;
  //CHECK: _Ptr<struct r> next;
};

struct p *sus(struct p *, struct p *);
//CHECK: struct p *sus(_Ptr<struct p> x, _Ptr<struct p> y) : itype(_Ptr<struct p>);

struct p *foo() {
  //CHECK: _Ptr<struct p> foo(void) {
  int ex1 = 2, ex2 = 3;
  struct p *x;
  //CHECK: _Ptr<struct p> x = ((void *)0);
  struct p *y;
  //CHECK: _Ptr<struct p> y = ((void *)0);
  x->x = &ex1;
  y->x = &ex2;
  x->y = &ex2;
  y->y = &ex1;
  struct p *z = (struct p *)sus(x, y);
  //CHECK: _Ptr<struct p> z = (_Ptr<struct p>)sus(x, y);
  return z;
}

struct p *bar() {
  //CHECK: _Ptr<struct p> bar(void) {
  int ex1 = 2, ex2 = 3;
  struct p *x;
  //CHECK: _Ptr<struct p> x = ((void *)0);
  struct p *y;
  //CHECK: _Ptr<struct p> y = ((void *)0);
  x->x = &ex1;
  y->x = &ex2;
  x->y = &ex2;
  y->y = &ex1;
  struct p *z = (struct p *)sus(x, y);
  //CHECK: _Ptr<struct p> z = (_Ptr<struct p>)sus(x, y);
  return z;
}

struct p *sus(struct p *x, struct p *y) {
  //CHECK: struct p *sus(_Ptr<struct p> x, _Ptr<struct p> y) : itype(_Ptr<struct p>) {
  x->y += 1;
  struct p *z = malloc(sizeof(struct p));
  //CHECK: struct p *z = malloc<struct p>(sizeof(struct p));
  z += 1;
  z->x = 1;
  z->y = 4;
  return z;
}
