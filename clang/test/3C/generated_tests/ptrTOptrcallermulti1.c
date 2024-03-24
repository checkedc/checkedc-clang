// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL %s %S/ptrTOptrcallermulti2.c --
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL %s %S/ptrTOptrcallermulti2.c --
// RUN: %clang -working-directory=%t.checkedNOALL -c ptrTOptrcallermulti1.c ptrTOptrcallermulti2.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %t.checkedNOALL/ptrTOptrcallermulti1.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %t.checkedALL/ptrTOptrcallermulti1.c %s
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %S/ptrTOptrcallermulti2.c %s --
// RUN: 3c -base-dir=%t.checked -alltypes -output-dir=%t.convert_again %t.checked/ptrTOptrcallermulti1.c %t.checked/ptrTOptrcallermulti2.c --
// RUN: test ! -f %t.convert_again/ptrTOptrcallermulti1.c
// RUN: test ! -f %t.convert_again/ptrTOptrcallermulti2.c

/******************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: having a pointer to a pointer*/
/*For robustness, this test is identical to
ptrTOptrprotocaller.c and ptrTOptrcaller.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
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

char ***sus(char ***, char ***);
//CHECK_NOALL: char ***sus(char ***x : itype(_Ptr<_Ptr<_Ptr<char>>>), _Ptr<_Ptr<_Ptr<char>>> y) : itype(_Ptr<char **>);
//CHECK_ALL: _Array_ptr<_Array_ptr<char *>> sus(char ***x : itype(_Ptr<_Ptr<_Ptr<char>>>), _Ptr<_Ptr<_Ptr<char>>> y) : count(5);

char ***foo() {
  //CHECK_NOALL: _Ptr<char **> foo(void) {
  //CHECK_ALL: _Ptr<_Array_ptr<char *>> foo(void) {
  char ***x = malloc(sizeof(char **));
  //CHECK: _Ptr<_Ptr<_Ptr<char>>> x = malloc<_Ptr<_Ptr<char>>>(sizeof(char **));
  char ***y = malloc(sizeof(char **));
  //CHECK: _Ptr<_Ptr<_Ptr<char>>> y = malloc<_Ptr<_Ptr<char>>>(sizeof(char **));
  char ***z = sus(x, y);
  //CHECK_NOALL: _Ptr<char **> z = sus(x, y);
  //CHECK_ALL: _Ptr<_Array_ptr<char *>> z = sus(x, y);
  return z;
}

char ***bar() {
  //CHECK_NOALL: char ***bar(void) : itype(_Ptr<char **>) {
  //CHECK_ALL: _Ptr<_Array_ptr<char *>> bar(void) {
  char ***x = malloc(sizeof(char **));
  //CHECK: _Ptr<_Ptr<_Ptr<char>>> x = malloc<_Ptr<_Ptr<char>>>(sizeof(char **));
  char ***y = malloc(sizeof(char **));
  //CHECK: _Ptr<_Ptr<_Ptr<char>>> y = malloc<_Ptr<_Ptr<char>>>(sizeof(char **));
  char ***z = sus(x, y);
  //CHECK_NOALL: char ***z = sus(x, y);
  //CHECK_ALL: _Array_ptr<_Array_ptr<char *>> __3c_lower_bound_z : count(5) = sus(x, y);
  //CHECK_ALL: _Array_ptr<_Array_ptr<char *>> z : bounds(__3c_lower_bound_z, __3c_lower_bound_z + 5) = __3c_lower_bound_z;
  z += 2;
  return z;
}
