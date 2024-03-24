// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/b18_bothstructp.c -- | diff %t.checked/b18_bothstructp.c -
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
  //CHECK_NOALL: int *x;
  //CHECK_ALL: _Array_ptr<int> x;
  char *y;
  //CHECK: _Ptr<char> y;
};

struct r {
  int data;
  struct r *next;
  //CHECK: _Ptr<struct r> next;
};

struct p sus(struct p x) {
  x.x += 1;
  struct p *n = malloc(sizeof(struct p));
  //CHECK: _Ptr<struct p> n = malloc<struct p>(sizeof(struct p));
  return *n;
}

struct p foo(void) {
  //CHECK_NOALL: struct p foo(void) {
  //CHECK_ALL: struct p foo(void) _Checked {
  struct p x;
  struct p z = sus(x);
  return z;
}

struct p bar(void) {
  //CHECK_NOALL: struct p bar(void) {
  //CHECK_ALL: struct p bar(void) _Checked {
  struct p x;
  struct p z = sus(x);
  z.x += 1;
  return z;
}
