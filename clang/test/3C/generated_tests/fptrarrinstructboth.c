// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/fptrarrinstructboth.c -- | diff %t.checked/fptrarrinstructboth.c -

/******************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: how the tool behaves when there is an array
  of function pointers in a struct*/
/*In this test, foo will treat its return value safely, but sus and bar will
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

struct arrfptr *sus(struct arrfptr *x, struct arrfptr *y) {
  //CHECK_NOALL: struct arrfptr *sus(struct arrfptr *x : itype(_Ptr<struct arrfptr>), _Ptr<struct arrfptr> y) : itype(_Ptr<struct arrfptr>) {
  //CHECK_ALL: struct arrfptr *sus(struct arrfptr *x : itype(_Ptr<struct arrfptr>), _Ptr<struct arrfptr> y) : itype(_Array_ptr<struct arrfptr>) {

  x = (struct arrfptr *)5;
  //CHECK: x = (struct arrfptr *)5;
  struct arrfptr *z = malloc(sizeof(struct arrfptr));
  //CHECK: struct arrfptr *z = malloc<struct arrfptr>(sizeof(struct arrfptr));
  int i;
  for (i = 0; i < 5; i++) {
    z->args[i] = i + 1;
  }
  z->funcs[0] = add1;
  z->funcs[1] = sub1;
  z->funcs[2] = zerohuh;
  z->funcs[3] = fib;
  z->funcs[4] = fact;

  z += 2;
  return z;
}

struct arrfptr *foo() {
  //CHECK: _Ptr<struct arrfptr> foo(void) {

  struct arrfptr *x = malloc(sizeof(struct arrfptr));
  //CHECK: _Ptr<struct arrfptr> x = malloc<struct arrfptr>(sizeof(struct arrfptr));
  struct arrfptr *y = malloc(sizeof(struct arrfptr));
  //CHECK: _Ptr<struct arrfptr> y = malloc<struct arrfptr>(sizeof(struct arrfptr));

  struct arrfptr *z = sus(x, y);
  //CHECK: _Ptr<struct arrfptr> z = sus(x, y);
  int i;
  for (i = 0; i < 5; i++) {
    //CHECK_NOALL: for (i = 0; i < 5; i++) {
    //CHECK_ALL: for (i = 0; i < 5; i++) _Checked {
    z->args[i] = z->funcs[i](z->args[i]);
  }

  return z;
}

struct arrfptr *bar() {
  //CHECK_NOALL: struct arrfptr *bar(void) : itype(_Ptr<struct arrfptr>) {
  //CHECK_ALL: _Ptr<struct arrfptr> bar(void) {

  struct arrfptr *x = malloc(sizeof(struct arrfptr));
  //CHECK: _Ptr<struct arrfptr> x = malloc<struct arrfptr>(sizeof(struct arrfptr));
  struct arrfptr *y = malloc(sizeof(struct arrfptr));
  //CHECK: _Ptr<struct arrfptr> y = malloc<struct arrfptr>(sizeof(struct arrfptr));

  struct arrfptr *z = sus(x, y);
  //CHECK_NOALL: struct arrfptr *z = sus(x, y);
  //CHECK_ALL: _Array_ptr<struct arrfptr> z = sus(x, y);
  int i;
  for (i = 0; i < 5; i++) {
    //CHECK_NOALL: for (i = 0; i < 5; i++) {
    //CHECK_ALL: for (i = 0; i < 5; i++) _Checked {
    z->args[i] = z->funcs[i](z->args[i]);
  }

  z += 2;
  return z;
}
