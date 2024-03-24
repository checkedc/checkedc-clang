// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/b16_callerpointerstruct.c -- | diff %t.checked/b16_callerpointerstruct.c -
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
  //CHECK: _Ptr<int> x;
  char *y;
  //CHECK: char *y;
};

struct r {
  int data;
  struct r *next;
  //CHECK: _Ptr<struct r> next;
};

struct p *sus(struct p *x, struct p *y) {
  //CHECK_NOALL: _Ptr<struct p> sus(_Ptr<struct p> x, _Ptr<struct p> y) {
  //CHECK_ALL: struct p *sus(_Ptr<struct p> x, _Ptr<struct p> y) : itype(_Array_ptr<struct p>) {
  x->y += 1;
  struct p *z = malloc(sizeof(struct p));
  //CHECK_NOALL: _Ptr<struct p> z = malloc<struct p>(sizeof(struct p));
  //CHECK_ALL: struct p *z = malloc<struct p>(sizeof(struct p));
  return z;
}

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
  //CHECK_NOALL: struct p *bar(void) : itype(_Ptr<struct p>) {
  //CHECK_ALL: _Ptr<struct p> bar(void) {
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
  //CHECK_NOALL: struct p *z = (struct p *)sus(x, y);
  //CHECK_ALL: _Array_ptr<struct p> z = (_Array_ptr<struct p>)sus(x, y);
  z += 2;
  return z;
}
