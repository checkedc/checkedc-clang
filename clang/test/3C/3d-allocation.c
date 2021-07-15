// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/3d-allocation.c -- | diff %t.checked/3d-allocation.c -

#include <stdio.h>
#include <stdlib.h>

int ***malloc3d(int y, int x, int z) {
  //CHECK_NOALL: int ***malloc3d(int y, int x, int z) : itype(_Ptr<int **>) {
  //CHECK_ALL: _Array_ptr<_Array_ptr<_Array_ptr<int>>> malloc3d(int y, int x, int z) : count(y) {

  int i, j;

  int ***t;
  //CHECK_NOALL: int ***t;
  //CHECK_ALL: _Array_ptr<_Array_ptr<_Array_ptr<int>>> t : count(y) = ((void *)0);

  t = malloc(y * sizeof(int *));
  //CHECK_NOALL: t = malloc<int **>(y * sizeof(int *));
  //CHECK_ALL: t = malloc<_Array_ptr<_Array_ptr<int>>>(y * sizeof(int *));

  for (i = 0; i < y; ++i) {

    t[i] = malloc(x * sizeof(int *));
    //CHECK_NOALL: t[i] = malloc<int *>(x * sizeof(int *));
    //CHECK_ALL: t[i] = malloc<_Array_ptr<int>>(x * sizeof(int *));

    for (j = 0; j < x; ++j) {

      t[i][j] = malloc(z * sizeof(int));
      //CHECK: t[i][j] = malloc<int>(z * sizeof(int));
    }
  }

  return t;
}

int main(void) {
  //CHECK_NOALL: int main(void) {
  //CHECK_ALL: int main(void) _Checked {

  int i, j, k;

  int x = 10;

  int y = 10;

  int z = 10;

  int ***t2 = malloc3d(y, x, z);
  //CHECK_NOALL: int ***t2 = malloc3d(y, x, z);
  //CHECK_ALL: _Array_ptr<_Array_ptr<_Array_ptr<int>>> t2 : count(y) = malloc3d(y, x, z);

  for (i = 0; i < y; ++i) {
    //CHECK_NOALL: for (i = 0; i < y; ++i) _Checked {
    //CHECK_ALL: for (i = 0; i < y; ++i) {

    for (j = 0; j < x; ++j) {

      for (k = 0; k < x; ++k) {
        //CHECK_NOALL: for (k = 0; k < x; ++k) _Unchecked {
        //CHECK_ALL: for (k = 0; k < x; ++k) {

        t2[i][j][k] = 1;
      }
    }
  }

  printf("3d Success\n");
  //CHECK_NOALL: printf("3d Success\n");
  //CHECK_ALL: _Unchecked { printf("3d Success\n"); };

  return 0;
}

void foo(int *x) {}
//CHECK: void foo(_Ptr<int> x) _Checked {}
