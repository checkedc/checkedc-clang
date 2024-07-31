// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/b28_structcastimplicit.c -- | diff %t.checked/b28_structcastimplicit.c -
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
  //CHECK: _Ptr<char> y;
};

struct r {
  int data;
  struct r *next;
  //CHECK: struct r *next;
};

struct r *sus(struct r x, struct r y) {
  //CHECK: _Ptr<struct r> sus(struct r x, struct r y) {
  x.next += 1;
  struct r *z = malloc(sizeof(struct r));
  //CHECK: _Ptr<struct r> z = malloc<struct r>(sizeof(struct r));
  z->data = 1;
  z->next = NULL;
  return z;
}

struct r *foo() {
  //CHECK: _Ptr<struct r> foo(void) {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct r *z = (struct r *)sus(x, y);
  //CHECK: _Ptr<struct r> z = (_Ptr<struct r>)sus(x, y);
  return z;
}

struct np *bar() {
  //CHECK: struct np *bar(void) : itype(_Ptr<struct np>) {
  struct r x, y;
  x.data = 2;
  y.data = 1;
  x.next = &y;
  y.next = &x;
  struct np *z = sus(x, y);
  //CHECK: struct np *z = ((struct r *)sus(x, y));
  return z;
}
