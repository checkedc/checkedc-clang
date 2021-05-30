/**
Test for parameter bounds with parameter with in the declaration.
Issue: https://github.com/correctcomputation/checkedc-clang/issues/573
**/

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o %t1.unusedl -
// RUN: 3c -base-dir=%S %s -- | %clang -c -fcheckedc-extension -x c -o %t2.unused -

#include <stdlib.h>

int gsize;
int *glob;
//CHECK_ALL: _Array_ptr<int> glob : count(gsize) = ((void *)0);

int foo(int *c, int z);
int foo(int *c, int l) {
  if (0 < l) return c[0];
  return 0;
}
//CHECK_ALL: int foo(_Array_ptr<int> c : count(z), int z);
//CHECK_ALL: int foo(_Array_ptr<int> c : count(l), int l) {

int consfoo(int *c);
int consfoo(int *c) {
  return c[0];
}
//CHECK_ALL: int consfoo(_Array_ptr<int> c : count(5));
//CHECK_ALL: int consfoo(_Array_ptr<int> c : count(5)) {

int globalfoo(int *c);
int globalfoo(int *c) {
  return c[0];
}
//CHECK_ALL: int globalfoo(_Array_ptr<int> c : count(gsize));
//CHECK_ALL: int globalfoo(_Array_ptr<int> c : count(gsize)) {

int caller() {
  int arr[5];
  gsize = 100;
  glob = malloc(gsize*sizeof(int));
  glob[0] = 1;
  globalfoo(glob);
  consfoo(arr);
  return 0;
}
//CHECK_ALL: int arr _Checked[5];
//CHECK_ALL: glob = malloc<int>(gsize*sizeof(int));
