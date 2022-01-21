// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL %s %S/arrinstructcalleemulti2.c --
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL %s %S/arrinstructcalleemulti2.c --
// RUN: %clang -working-directory=%t.checkedNOALL -c arrinstructcalleemulti1.c arrinstructcalleemulti2.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %t.checkedNOALL/arrinstructcalleemulti1.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %t.checkedALL/arrinstructcalleemulti1.c %s
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %S/arrinstructcalleemulti2.c %s --
// RUN: 3c -base-dir=%t.checked -alltypes -output-dir=%t.convert_again %t.checked/arrinstructcalleemulti1.c %t.checked/arrinstructcalleemulti2.c --
// RUN: test ! -f %t.convert_again/arrinstructcalleemulti1.c
// RUN: test ! -f %t.convert_again/arrinstructcalleemulti2.c

/******************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: how the tool behaves when there is an array
  field within a struct*/
/*For robustness, this test is identical to
arrinstructprotocallee.c and arrinstructcallee.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
/*In this test, foo and bar will treat their return values safely, but sus will
  not, through invalid pointer arithmetic, an unsafe cast, etc*/

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

static int add1(int x) {
  //CHECK: static int add1(int x) _Checked {
  return x + 1;
}

static int sub1(int x) {
  //CHECK: static int sub1(int x) _Checked {
  return x - 1;
}

static int fact(int n) {
  //CHECK: static int fact(int n) _Checked {
  if (n == 0) {
    return 1;
  }
  return n * fact(n - 1);
}

static int fib(int n) {
  //CHECK: static int fib(int n) _Checked {
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }
  return fib(n - 1) + fib(n - 2);
}

static int zerohuh(int n) {
  //CHECK: static int zerohuh(int n) _Checked {
  return !n;
}

static int *mul2(int *x) {
  //CHECK: static _Ptr<int> mul2(_Ptr<int> x) _Checked {
  *x *= 2;
  return x;
}

struct warr *sus(struct warr *, struct warr *);
//CHECK_NOALL: struct warr *sus(struct warr *x : itype(_Ptr<struct warr>), struct warr *y : itype(_Ptr<struct warr>)) : itype(_Ptr<struct warr>);
//CHECK_ALL: _Ptr<struct warr> sus(struct warr *x : itype(_Ptr<struct warr>), _Array_ptr<struct warr> y);

struct warr *foo() {
  //CHECK: _Ptr<struct warr> foo(void) {
  struct warr *x = malloc(sizeof(struct warr));
  //CHECK: _Ptr<struct warr> x = malloc<struct warr>(sizeof(struct warr));
  struct warr *y = malloc(sizeof(struct warr));
  //CHECK_NOALL: _Ptr<struct warr> y = malloc<struct warr>(sizeof(struct warr));
  //CHECK_ALL: struct warr *y = malloc<struct warr>(sizeof(struct warr));
  struct warr *z = sus(x, y);
  //CHECK_NOALL: _Ptr<struct warr> z = sus(x, y);
  //CHECK_ALL: _Ptr<struct warr> z = sus(x, _Assume_bounds_cast<_Array_ptr<struct warr>>(y, bounds(unknown)));
  return z;
}

struct warr *bar() {
  //CHECK: _Ptr<struct warr> bar(void) {
  struct warr *x = malloc(sizeof(struct warr));
  //CHECK: _Ptr<struct warr> x = malloc<struct warr>(sizeof(struct warr));
  struct warr *y = malloc(sizeof(struct warr));
  //CHECK_NOALL: _Ptr<struct warr> y = malloc<struct warr>(sizeof(struct warr));
  //CHECK_ALL: struct warr *y = malloc<struct warr>(sizeof(struct warr));
  struct warr *z = sus(x, y);
  //CHECK_NOALL: _Ptr<struct warr> z = sus(x, y);
  //CHECK_ALL: _Ptr<struct warr> z = sus(x, _Assume_bounds_cast<_Array_ptr<struct warr>>(y, bounds(unknown)));
  return z;
}
