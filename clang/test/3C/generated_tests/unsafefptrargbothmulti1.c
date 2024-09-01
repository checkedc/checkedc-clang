// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL %s %S/unsafefptrargbothmulti2.c --
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL %s %S/unsafefptrargbothmulti2.c --
// RUN: %clang -working-directory=%t.checkedNOALL -c unsafefptrargbothmulti1.c unsafefptrargbothmulti2.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %t.checkedNOALL/unsafefptrargbothmulti1.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %t.checkedALL/unsafefptrargbothmulti1.c %s
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %S/unsafefptrargbothmulti2.c %s --
// RUN: 3c -base-dir=%t.checked -alltypes -output-dir=%t.convert_again %t.checked/unsafefptrargbothmulti1.c %t.checked/unsafefptrargbothmulti2.c --
// RUN: test ! -f %t.convert_again/unsafefptrargbothmulti1.c
// RUN: test ! -f %t.convert_again/unsafefptrargbothmulti2.c

/******************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: passing a function pointer as an argument to a
  function unsafely (by casting it unsafely)*/
/*For robustness, this test is identical to
unsafefptrargprotoboth.c and unsafefptrargboth.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
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
//CHECK_ALL: _Array_ptr<int> sus(int ((*x)(int)) : itype(_Ptr<int (int)>), _Ptr<int (int)> y);

int *foo() {
  //CHECK_NOALL: _Ptr<int> foo(void) {
  //CHECK_ALL: _Array_ptr<int> foo(void) {

  int (*x)(int) = add1;
  //CHECK: _Ptr<int (int)> x = add1;
  int (*y)(int) = mul2;
  //CHECK: int (*y)(int) = mul2;
  int *z = sus(x, y);
  //CHECK_NOALL: _Ptr<int> z = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));
  //CHECK_ALL: _Array_ptr<int> z = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));

  return z;
}

int *bar() {
  //CHECK_NOALL: int *bar(void) : itype(_Ptr<int>) {
  //CHECK_ALL: _Array_ptr<int> bar(void) {

  int (*x)(int) = add1;
  //CHECK: _Ptr<int (int)> x = add1;
  int (*y)(int) = mul2;
  //CHECK: int (*y)(int) = mul2;
  int *z = sus(x, y);
  //CHECK_NOALL: int *z = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));
  //CHECK_ALL: _Array_ptr<int> z = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));

  z += 2;
  return z;
}
