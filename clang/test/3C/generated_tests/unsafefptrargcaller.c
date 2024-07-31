// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- -Wno-error=incompatible-function-pointer-types | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=incompatible-function-pointer-types | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=incompatible-function-pointer-types | %clang -c -Wno-error=incompatible-function-pointer-types -fcheckedc-extension -x c -o /dev/null -

// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s -- -Wno-error=incompatible-function-pointer-types
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/unsafefptrargcaller.c -- -Wno-error=incompatible-function-pointer-types | diff %t.checked/unsafefptrargcaller.c -

/******************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: passing a function pointer as an argument to a
  function unsafely (by casting it unsafely)*/
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
  //CHECK: int *mul2(int *x) {
  *x *= 2;
  return x;
}

int *sus(int (*x)(int), int (*y)(int)) {
  //CHECK_NOALL: int *sus(int ((*x)(int)) : itype(_Ptr<int (int)>), _Ptr<int (int)> y) : itype(_Ptr<int>) {
  //CHECK_ALL: _Array_ptr<int> sus(int ((*x)(int)) : itype(_Ptr<int (int)>), _Ptr<int (int)> y) : count(5) {

  x = (int (*)(int))5;
  //CHECK: x = (int (*)(int))5;
  int *z = calloc(5, sizeof(int));
  //CHECK_NOALL: int *z = calloc<int>(5, sizeof(int));
  //CHECK_ALL: _Array_ptr<int> z : count(5) = calloc<int>(5, sizeof(int));
  int i;
  for (i = 0; i < 5; i++) {
    //CHECK_NOALL: for (i = 0; i < 5; i++) {
    //CHECK_ALL: for (i = 0; i < 5; i++) _Checked {
    z[i] = y(i);
  }

  return z;
}

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
  //CHECK_NOALL: int *bar(void) : itype(_Ptr<int>) {
  //CHECK_ALL: _Array_ptr<int> bar(void) {

  int (*x)(int) = add1;
  //CHECK: _Ptr<int (int)> x = add1;
  int (*y)(int) = mul2;
  //CHECK: int (*y)(int) = mul2;
  int *z = sus(x, y);
  //CHECK_NOALL: int *z = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));
  //CHECK_ALL: _Array_ptr<int> __3c_lower_bound_z : count(5) = sus(x, _Assume_bounds_cast<_Ptr<int (int)>>(y));
  //CHECK_ALL: _Array_ptr<int> z : bounds(__3c_lower_bound_z, __3c_lower_bound_z + 5) = __3c_lower_bound_z;

  z += 2;
  return z;
}
