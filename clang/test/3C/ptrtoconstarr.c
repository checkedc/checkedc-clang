// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S --addcr %s -- | %clang -c  -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S --addcr --alltypes %s -- | %clang -c  -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/ptrtoconstarr.c -- | diff %t.checked/ptrtoconstarr.c -

#include <string.h>

int bar(void) {
  //CHECK_ALL: int bar(void) _Checked {
  //CHECK_NOALL: int bar(void) {
  int local[10];
  //CHECK_ALL: int local _Checked[10];
  //CHECK_NOALL: int local[10];
  int(*coef)[10] = &local;
  //CHECK_ALL: _Ptr<int _Checked[10]> coef = &local;
  //CHECK_NOALL: _Ptr<int[10]> coef = &local;

  //int two[5][10];
  //DONTCHECK_ALL: int two _Checked[5] _Checked[10];
  //int (*two_ptr)[5][10];

  return (*coef)[1];
}

struct ex {
  int a;
  int *ptr;
  int (*ca)[10];
};
//CHECK: _Ptr<int> ptr;
//CHECK_ALL: _Ptr<int _Checked[10]> ca;
//CHECK_NOALL: _Ptr<int[10]> ca;

int foo(void) {
  //CHECK_ALL: int foo(void) _Checked {
  //CHECK_NOALL: int foo(void) {
  int local[10];
  //CHECK_ALL: int local _Checked[10];
  //CHECK_NOALL: int local[10];
  int y = 2;
  struct ex e = {3, &y, &local};
  y += (*e.ca)[3];
  return *e.ptr;
}

int baz(void) {
  //CHECK_ALL: int baz(void) _Checked {
  //CHECK_NOALL: int baz(void) {
  char local[5] = "test";
  //CHECK_ALL: char local _Nt_checked[5] =  "test";
  //CHECK_NOALL: char local[5] = "test";
  char(*ptr)[5] = &local;
  //CHECK_ALL: _Ptr<char _Nt_checked[5]> ptr = &local;
  //CHECK_NOALL: _Ptr<char[5]> ptr = &local;

  return strlen(*ptr);
}

struct pair {
  int fst;
  int snd;
};

int sum10pairs(struct pair (*pairs)[10]) {
  //CHECK_ALL: int sum10pairs(_Ptr<struct pair _Checked[10]> pairs) _Checked {
  //CHECK_NOALL: int sum10pairs(_Ptr<struct pair[10]> pairs) {
  int sum = 0;

  for (int i = 0; i < 10; i++) {
    struct pair this = (*pairs)[i];
    sum += this.fst + this.snd;
  }

  return sum;
}

typedef int (*compl )[5];
//CHECK_ALL: typedef _Ptr<int _Checked[5]> compl;
//CHECK_NOALL: typedef _Ptr<int[5]> compl;

int example(void) {
  int local[5] = {0};
  //CHECK_ALL: int local _Checked[5] = {0};
  //CHECK_NOALL: int local[5] = {0};
  compl t = &local;
  //CHECK_ALL: compl t = &local;
  //CHECK_NOALL: compl t = &local;
  return (*t)[2];
}
