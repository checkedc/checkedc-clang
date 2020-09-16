// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/3d-allocation.checked.c -- | count 0
// RUN: rm %S/3d-allocation.checked.c

#include <stdio.h>
#include <stdlib.h>

extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

int ***malloc3d(int y, int x, int z) {
	//CHECK_NOALL: int ***malloc3d(int y, int x, int z) {
	//CHECK_ALL: _Array_ptr<_Array_ptr<_Array_ptr<int>>> malloc3d(int y, int x, int z) {

	int i, j;

	int ***t;
	//CHECK_NOALL: int ***t;
	//CHECK_ALL: 	_Array_ptr<_Array_ptr<_Array_ptr<int>>> t : count(y) = ((void *)0);



	t = malloc(y * sizeof(int *));
	//CHECK_NOALL: t = malloc<int **>(y * sizeof(int *));
	//CHECK_ALL: 	t = malloc<_Array_ptr<_Array_ptr<int>>>(y * sizeof(int *));



	for (i = 0; i < y; ++i) {

		t[i] = malloc(x * sizeof(int *));
	//CHECK_NOALL: t[i] = malloc<int *>(x * sizeof(int *));
	//CHECK_ALL: 		t[i] = malloc<_Array_ptr<int>>(x * sizeof(int *));

		for (j = 0; j < x; ++j) {

			t[i][j] = malloc(z * sizeof(int));
	//CHECK: t[i][j] = malloc<int>(z * sizeof(int));

		}

	}



	return t;

}

int main(void) {
  //CHECK_ALL: int main(void) _Checked {

	int i, j, k;

	int x = 10;

	int y = 10;

	int z = 10;



	int ***t2 = malloc3d(y, x, z);
	//CHECK_NOALL: int ***t2 = malloc3d(y, x, z);
	//CHECK_ALL: 	_Array_ptr<_Array_ptr<_Array_ptr<int>>> t2 =  malloc3d(y, x, z);


	for (i = 0; i < y; ++i) {
	//CHECK_NOALL: for (i = 0; i < y; ++i) _Checked {

		for (j = 0; j < x; ++j) {

			for (k = 0; k < x; ++k) {
	//CHECK_NOALL: for (k = 0; k < x; ++k) _Unchecked {
	//CHECK_ALL: 			for (k = 0; k < x; ++k) {

				t2[i][j][k] = 1;

			}

		}

	}

	printf("3d Success\n");
  //CHECK_ALL: _Unchecked { printf("3d Success\n"); };



	return 0;

}

void foo(int *x) {}
	//CHECK: void foo(_Ptr<int> x) _Checked {}
