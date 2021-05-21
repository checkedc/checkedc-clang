// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/b30_structprotoconflictbodyconvert.c -- | diff %t.checked/b30_structprotoconflictbodyconvert.c -
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
  //CHECK_NOALL: struct r *next;
  //CHECK_ALL: _Array_ptr<struct r> next;
};

struct np *sus(struct r *, struct r *);
//CHECK: _Ptr<struct np> sus(_Ptr<struct r> x, _Ptr<struct r> y);

struct r *foo() {
  //CHECK: struct r *foo(void) : itype(_Ptr<struct r>) {
  struct r *x;
  //CHECK_NOALL: struct r *x;
  //CHECK_ALL: _Array_ptr<struct r> x = ((void *)0);
  struct r *y;
  //CHECK_NOALL: struct r *y;
  //CHECK_ALL: _Array_ptr<struct r> y = ((void *)0);
  x->data = 2;
  y->data = 1;
  x->next = y;
  y->next = x;
  struct r *z = (struct r *)sus(x, y);
  //CHECK_NOALL: struct r *z = (struct r *)sus(_Assume_bounds_cast<_Ptr<struct r>>(x), _Assume_bounds_cast<_Ptr<struct r>>(y));
  //CHECK_ALL: struct r *z = (struct r *)sus(x, y);
  return z;
}

struct np *bar() {
  //CHECK_NOALL: _Ptr<struct np> bar(void) {
  //CHECK_ALL: _Ptr<struct np> bar(void) _Checked {
  struct r *x;
  //CHECK_NOALL: struct r *x;
  //CHECK_ALL: _Array_ptr<struct r> x = ((void *)0);
  struct r *y;
  //CHECK_NOALL: struct r *y;
  //CHECK_ALL: _Array_ptr<struct r> y = ((void *)0);
  x->data = 2;
  y->data = 1;
  x->next = y;
  y->next = x;
  struct np *z = sus(x, y);
  //CHECK_NOALL: _Ptr<struct np> z = sus(_Assume_bounds_cast<_Ptr<struct r>>(x), _Assume_bounds_cast<_Ptr<struct r>>(y));
  //CHECK_ALL: _Ptr<struct np> z = sus(x, y);
  return z;
}

struct np *sus(struct r *x, struct r *y) {
  //CHECK: _Ptr<struct np> sus(_Ptr<struct r> x, _Ptr<struct r> y) {
  x->next += 1;
  struct np *z = malloc(sizeof(struct np));
  //CHECK: _Ptr<struct np> z = malloc<struct np>(sizeof(struct np));
  z->x = 1;
  z->y = 0;
  return z;
}
