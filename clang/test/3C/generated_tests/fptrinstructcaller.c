// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/fptrinstructcaller.c -- | diff %t.checked/fptrinstructcaller.c -

/******************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: how the tool behaves when a function pointer
  is a field of a struct*/
/*In this test, foo and sus will treat their return values safely, but bar will
  not, through invalid pointer arithmetic, an unsafe cast, etc.*/

/******************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct general {
  int data;
  struct general *next;
  //CHECK: _Ptr<struct general> next;
};

struct warr {
  int data1[5];
  //CHECK_NOALL: int data1[5];
  //CHECK_ALL: int data1 _Checked[5];
  char *name;
  //CHECK: _Ptr<char> name;
};

struct fptrarr {
  int *values;
  //CHECK: _Ptr<int> values;
  char *name;
  //CHECK: _Ptr<char> name;
  int (*mapper)(int);
  //CHECK: _Ptr<int (int)> mapper;
};

struct fptr {
  int *value;
  //CHECK: _Ptr<int> value;
  int (*func)(int);
  //CHECK: _Ptr<int (int)> func;
};

struct arrfptr {
  int args[5];
  //CHECK_NOALL: int args[5];
  //CHECK_ALL: int args _Checked[5];
  int (*funcs[5])(int);
  //CHECK_NOALL: int (*funcs[5])(int);
  //CHECK_ALL: _Ptr<int (int)> funcs _Checked[5];
};

int add1(int x) {
  //CHECK: int add1(int x) _Checked {
  return x + 1;
}

int sub1(int x) {
  //CHECK: int sub1(int x) _Checked {
  return x - 1;
}

int fact(int n) {
  //CHECK: int fact(int n) _Checked {
  if (n == 0) {
    return 1;
  }
  return n * fact(n - 1);
}

int fib(int n) {
  //CHECK: int fib(int n) _Checked {
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }
  return fib(n - 1) + fib(n - 2);
}

int zerohuh(int n) {
  //CHECK: int zerohuh(int n) _Checked {
  return !n;
}

int *mul2(int *x) {
  //CHECK: _Ptr<int> mul2(_Ptr<int> x) _Checked {
  *x *= 2;
  return x;
}

struct fptr *sus(struct fptr *x, struct fptr *y) {
  //CHECK_NOALL: _Ptr<struct fptr> sus(struct fptr *x : itype(_Ptr<struct fptr>), _Ptr<struct fptr> y) {
  //CHECK_ALL: struct fptr *sus(struct fptr *x : itype(_Ptr<struct fptr>), _Ptr<struct fptr> y) : itype(_Array_ptr<struct fptr>) {

  x = (struct fptr *)5;
  //CHECK: x = (struct fptr *)5;
  struct fptr *z = malloc(sizeof(struct fptr));
  //CHECK_NOALL: _Ptr<struct fptr> z = malloc<struct fptr>(sizeof(struct fptr));
  //CHECK_ALL: struct fptr *z = malloc<struct fptr>(sizeof(struct fptr));
  z->value = y->value;
  z->func = fact;

  return z;
}

struct fptr *foo() {
  //CHECK: _Ptr<struct fptr> foo(void) {

  struct fptr *x = malloc(sizeof(struct fptr));
  //CHECK: _Ptr<struct fptr> x = malloc<struct fptr>(sizeof(struct fptr));
  struct fptr *y = malloc(sizeof(struct fptr));
  //CHECK: _Ptr<struct fptr> y = malloc<struct fptr>(sizeof(struct fptr));
  struct fptr *z = sus(x, y);
  //CHECK: _Ptr<struct fptr> z = sus(x, y);

  return z;
}

struct fptr *bar() {
  //CHECK_NOALL: struct fptr *bar(void) : itype(_Ptr<struct fptr>) {
  //CHECK_ALL: _Ptr<struct fptr> bar(void) {

  struct fptr *x = malloc(sizeof(struct fptr));
  //CHECK: _Ptr<struct fptr> x = malloc<struct fptr>(sizeof(struct fptr));
  struct fptr *y = malloc(sizeof(struct fptr));
  //CHECK: _Ptr<struct fptr> y = malloc<struct fptr>(sizeof(struct fptr));
  struct fptr *z = sus(x, y);
  //CHECK_NOALL: struct fptr *z = ((struct fptr *)sus(x, y));
  //CHECK_ALL: _Array_ptr<struct fptr> z = sus(x, y);

  z += 2;
  return z;
}
