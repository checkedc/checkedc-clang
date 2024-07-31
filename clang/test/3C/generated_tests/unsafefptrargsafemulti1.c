// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL %s %S/unsafefptrargsafemulti2.c -- -Wno-error=incompatible-function-pointer-types
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL %s %S/unsafefptrargsafemulti2.c -- -Wno-error=incompatible-function-pointer-types
// RUN: %clang -working-directory=%t.checkedNOALL -c -Wno-error=incompatible-function-pointer-types unsafefptrargsafemulti1.c unsafefptrargsafemulti2.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %t.checkedNOALL/unsafefptrargsafemulti1.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %t.checkedALL/unsafefptrargsafemulti1.c %s
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %S/unsafefptrargsafemulti2.c %s -- -Wno-error=incompatible-function-pointer-types
// RUN: 3c -base-dir=%t.checked -alltypes -output-dir=%t.convert_again %t.checked/unsafefptrargsafemulti1.c %t.checked/unsafefptrargsafemulti2.c -- -Wno-error=incompatible-function-pointer-types
// RUN: test ! -f %t.convert_again/unsafefptrargsafemulti1.c
// RUN: test ! -f %t.convert_again/unsafefptrargsafemulti2.c

/******************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: passing a function pointer as an argument to a
  function unsafely (by casting it unsafely)*/
/*For robustness, this test is identical to
unsafefptrargprotosafe.c and unsafefptrargsafe.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
/*In this test, foo, bar, and sus will all treat their return values safely*/

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
  //CHECK: static int *mul2(int *x) {
  *x *= 2;
  return x;
}

int *sus(int (*)(int), int (*)(int));
//CHECK_NOALL: int *sus(int ((*x)(int)) : itype(_Ptr<int (int)>), _Ptr<int (int)> y) : itype(_Ptr<int>);
//CHECK_ALL: _Array_ptr<int> sus(int ((*x)(int)) : itype(_Ptr<int (int)>), _Ptr<int (int)> y) : count(5);

int *foo() {
  //CHECK_NOALL: _Ptr<int> foo(void) {
  //CHECK_ALL: _Array_ptr<int> foo(void) : count(5) {

  int (*x)(int) = add1;
  //CHECK: _Ptr<int (int)> x = add1;
  int (*y)(int) = mul2;
  //CHECK: int (*y)(int) = mul2;
  int *z = sus(x, y);
  //CHECK_NOALL: _Ptr<int> z = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));
  //CHECK_ALL: _Array_ptr<int> z : count(5) = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));

  return z;
}

int *bar() {
  //CHECK_NOALL: _Ptr<int> bar(void) {
  //CHECK_ALL: _Array_ptr<int> bar(void) : count(5) {

  int (*x)(int) = add1;
  //CHECK: _Ptr<int (int)> x = add1;
  int (*y)(int) = mul2;
  //CHECK: int (*y)(int) = mul2;
  int *z = sus(x, y);
  //CHECK_NOALL: _Ptr<int> z = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));
  //CHECK_ALL: _Array_ptr<int> z : count(5) = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));

  return z;
}
