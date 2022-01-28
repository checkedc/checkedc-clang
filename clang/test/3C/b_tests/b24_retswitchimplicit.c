// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/b24_retswitchimplicit.c -- | diff %t.checked/b24_retswitchimplicit.c -
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *sus(int *x, int *y) {
  //CHECK_NOALL: _Ptr<char> sus(int *x : itype(_Ptr<int>), _Ptr<int> y) {
  //CHECK_ALL: _Ptr<char> sus(_Array_ptr<int> x, _Ptr<int> y) {
  char *z = malloc(sizeof(char));
  //CHECK: _Ptr<char> z = malloc<char>(sizeof(char));
  *z = 1;
  x++;
  *x = 2;
  return z;
}

char *foo() {
  //CHECK: char *foo(void) : itype(_Ptr<char>) {
  int sx = 3, sy = 4;
  int *x = &sx;
  //CHECK_NOALL: _Ptr<int> x = &sx;
  //CHECK_ALL: int *x = &sx;
  int *y = &sy;
  //CHECK: _Ptr<int> y = &sy;
  char *z = (int *)sus(x, y);
  //CHECK_NOALL: char *z = (int *)sus(x, y);
  //CHECK_ALL: char *z = (int *)sus(_Assume_bounds_cast<_Array_ptr<int>>(x, bounds(unknown)), y);
  *z = *z + 1;
  return z;
}

int *bar() {
  //CHECK: int *bar(void) : itype(_Ptr<int>) {
  int sx = 3, sy = 4;
  int *x = &sx;
  //CHECK_NOALL: _Ptr<int> x = &sx;
  //CHECK_ALL: int *x = &sx;
  int *y = &sy;
  //CHECK: _Ptr<int> y = &sy;
  int *z = sus(x, y);
  //CHECK_NOALL: int *z = ((char *)sus(x, y));
  //CHECK_ALL: int *z = ((char *)sus(_Assume_bounds_cast<_Array_ptr<int>>(x, bounds(unknown)), y));
  return z;
}
