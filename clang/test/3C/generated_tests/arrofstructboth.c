// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/arrofstructboth.c -- | diff %t.checked/arrofstructboth.c -

/******************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: how the tool behaves when there is an array
  of structs*/
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
  //CHECK_NOALL: struct general *next;
  //CHECK_ALL: _Ptr<struct general> next;
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

struct general **sus(struct general *x, struct general *y) {
  //CHECK_NOALL: struct general **sus(struct general *x : itype(_Ptr<struct general>), struct general *y : itype(_Ptr<struct general>)) : itype(_Ptr<struct general *>) {
  //CHECK_ALL: _Array_ptr<_Ptr<struct general>> sus(struct general *x : itype(_Ptr<struct general>), _Ptr<struct general> y) {
  x = (struct general *)5;
  //CHECK: x = (struct general *)5;
  struct general **z = calloc(5, sizeof(struct general *));
  //CHECK_NOALL: struct general **z = calloc<struct general *>(5, sizeof(struct general *));
  //CHECK_ALL: _Array_ptr<_Ptr<struct general>> __3c_lower_bound_z : count(5) = calloc<_Ptr<struct general>>(5, sizeof(struct general *));
  //CHECK_ALL: _Array_ptr<_Ptr<struct general>> z : bounds(__3c_lower_bound_z, __3c_lower_bound_z + 5) = __3c_lower_bound_z;
  struct general *curr = y;
  //CHECK_NOALL: struct general *curr = y;
  //CHECK_ALL: _Ptr<struct general> curr = y;
  int i;
  for (i = 0; i < 5; i++) {
    //CHECK_NOALL: for (i = 0; i < 5; i++) {
    //CHECK_ALL: for (i = 0; i < 5; i++) _Checked {
    z[i] = curr;
    curr = curr->next;
  }

  z += 2;
  return z;
}

struct general **foo() {
  //CHECK_NOALL: _Ptr<struct general *> foo(void) {
  //CHECK_ALL: _Array_ptr<_Ptr<struct general>> foo(void) {
  struct general *x = malloc(sizeof(struct general));
  //CHECK: _Ptr<struct general> x = malloc<struct general>(sizeof(struct general));
  struct general *y = malloc(sizeof(struct general));
  //CHECK_NOALL: struct general *y = malloc<struct general>(sizeof(struct general));
  //CHECK_ALL: _Ptr<struct general> y = malloc<struct general>(sizeof(struct general));

  struct general *curr = y;
  //CHECK_NOALL: struct general *curr = y;
  //CHECK_ALL: _Ptr<struct general> curr = y;
  int i;
  for (i = 1; i < 5; i++, curr = curr->next) {
    curr->data = i;
    curr->next = malloc(sizeof(struct general));
    curr->next->data = i + 1;
  }
  struct general **z = sus(x, y);
  //CHECK_NOALL: _Ptr<struct general *> z = sus(x, y);
  //CHECK_ALL: _Array_ptr<_Ptr<struct general>> z = sus(x, y);
  return z;
}

struct general **bar() {
  //CHECK_NOALL: struct general **bar(void) : itype(_Ptr<struct general *>) {
  //CHECK_ALL: _Array_ptr<_Ptr<struct general>> bar(void) {
  struct general *x = malloc(sizeof(struct general));
  //CHECK: _Ptr<struct general> x = malloc<struct general>(sizeof(struct general));
  struct general *y = malloc(sizeof(struct general));
  //CHECK_NOALL: struct general *y = malloc<struct general>(sizeof(struct general));
  //CHECK_ALL: _Ptr<struct general> y = malloc<struct general>(sizeof(struct general));

  struct general *curr = y;
  //CHECK_NOALL: struct general *curr = y;
  //CHECK_ALL: _Ptr<struct general> curr = y;
  int i;
  for (i = 1; i < 5; i++, curr = curr->next) {
    curr->data = i;
    curr->next = malloc(sizeof(struct general));
    curr->next->data = i + 1;
  }
  struct general **z = sus(x, y);
  //CHECK_NOALL: struct general **z = sus(x, y);
  //CHECK_ALL: _Array_ptr<_Ptr<struct general>> z = sus(x, y);
  z += 2;
  return z;
}
