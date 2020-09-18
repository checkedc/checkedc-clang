// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stdio.h>
#include <stdlib.h>

extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

int ***malloc3d(int y, int x, int z) {

	int i, j;

	int ***t;



	t = malloc(y * sizeof(int *));



	for (i = 0; i < y; ++i) {

		t[i] = malloc(x * sizeof(int *));

		for (j = 0; j < x; ++j) {

			t[i][j] = malloc(z * sizeof(int));

		}

	}



	return t;

}
//CHECK: int *** malloc3d(int y, int x, int z) {
//CHECK: int ***t;


int main(void) {

	int i, j, k;

	int x = 10;

	int y = 10;

	int z = 10;



	int ***t2 = malloc3d(y, x, z);

	for (i = 0; i < y; ++i) {

		for (j = 0; j < x; ++j) {

			for (k = 0; k < x; ++k) {

				t2[i][j][k] = 1;

			}

		}

	}

	printf("3d Success\n");



	return 0;

}
//CHECK: int ***t2 = malloc3d(y, x, z);
