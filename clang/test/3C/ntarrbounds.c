/**
Tests for ntarr pointer bounds.
Issue: https://github.com/correctcomputation/checkedc-clang/issues/553
**/

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o %t1.unusedl -
// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S %s -- | %clang -c -fcheckedc-extension -x c -o %t2.unused -


#include <string_checked.h>
void foo(char *p, int x) {
  char *q = p;
  if (x)
    q = strdup("hello"); // len("hello") < 15
  else
    x = strlen(q);
}
//CHECK_NOALL: void foo(char *p : itype(_Ptr<char>), int x) {
//CHECK_ALL: void foo(_Nt_array_ptr<char> p, int x) {
//CHECK_ALL: _Nt_array_ptr<char> q = p;
//CHECK_ALL: q = ((_Nt_array_ptr<char> )strdup("hello")); // len("hello") < 15

void bar(void) {
  char buf[15];
  foo(buf,1);
}
//CHECK_ALL: char buf _Nt_checked[15];

char *baz(void) {
  char *p;
  p = strdup("baz");
  return p;
}
//CHECK_NOALL: char *baz(void) : itype(_Ptr<char>) {
//CHECK_ALL: _Nt_array_ptr<char> baz(void) {
//CHECK_ALL: _Nt_array_ptr<char> p = ((void *)0);
//CHECK_ALL: p = ((_Nt_array_ptr<char> )strdup("baz"));
