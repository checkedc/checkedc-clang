// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK,CHECK_NOALL" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK,CHECK_ALL" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/arrboundsheuristicsitype.c -- | diff %t.checked/arrboundsheuristicsitype.c -

// Verify that array bounds heuristics are correctly applied to parameters with
// itypes.

void foo(void *);

void test0(int *a, int len) {
//CHECK_ALL: void test0(int *a : itype(_Array_ptr<int>) count(len), int len) {
//CHECK_NOALL: void test0(int *a : itype(_Ptr<int>), int len) {
  foo(a);
  (void) a[len - 1];
}

void test1(int *a : itype(_Array_ptr<int>), int len) {
//CHECK_ALL: void test1(int *a : itype(_Array_ptr<int>) count(len), int len) {
//CHECK_NOALL: void test1(int *a : itype(_Ptr<int>), int len) {
  foo(a);
  (void) a[len - 1];
}

#include<stdlib.h>

void test2(int *a, int len) {
//CHECK_ALL: void test2(int *a : itype(_Array_ptr<int>) count(len), int len) {
//CHECK_NOALL: void test2(int *a : itype(_Ptr<int>), int len) {
  a = malloc(sizeof(int)*len);
  foo(a);
  (void) a[len - 1];
}
