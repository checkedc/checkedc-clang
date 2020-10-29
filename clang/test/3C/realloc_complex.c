// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/realloc_complex.checked.c -- | count 0
// RUN: rm %S/realloc_complex.checked.c

/**********************************************************/
/* This file tests the conversion tool's behavior with    */
/* multiple complex realloc calls                         */
/**********************************************************/

#include <stddef.h>
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

void foo(int *count) { 
	//CHECK: void foo(_Ptr<int> count) { 
    /*using a as an array in both the malloc and realloc*/
    int *a = malloc(2*sizeof(int));  
	//CHECK_NOALL: int *a = malloc<int>(2*sizeof(int));  
	//CHECK_ALL:     _Array_ptr<int> a : count(2) =  malloc<int>(2*sizeof(int));  
    a[1] = 4; 
    a = realloc(a, sizeof(int)*(*count));
	//CHECK: a = realloc<int>(a, sizeof(int)*(*count));
    a[2] = 2;

    /*using b as a pointer here*/
    int *b = malloc(sizeof(int));  
	//CHECK: int *b = malloc<int>(sizeof(int));  
    *b = 3;
    /*now using it as an array here, should be wild*/
    b = realloc(b, sizeof(int)*(*count));
	//CHECK: b = realloc<int>(b, sizeof(int)*(*count));
    b[2] = 2;

    /*  what follows are variations of the above, but instead 
        using two separate pointers for the malloc and realloc */
	//CHECK: using two separate pointers for the malloc and realloc */
    int *y = malloc(2*sizeof(int)); 
	//CHECK_NOALL: int *y = malloc<int>(2*sizeof(int)); 
	//CHECK_ALL:     _Array_ptr<int> y : count(2) =  malloc<int>(2*sizeof(int)); 
    int *w = malloc(sizeof(int));
	//CHECK: int *w = malloc<int>(sizeof(int));
    y[1] = 3;
    int *z = realloc(y, 5*sizeof(int));
	//CHECK_NOALL: int *z = realloc<int>(y, 5*sizeof(int));
	//CHECK_ALL:     _Array_ptr<int> z =  realloc<int>(y, 5*sizeof(int));
    int *m = realloc(w, 2*sizeof(int)); 
	//CHECK_NOALL: int *m = realloc<int>(w, 2*sizeof(int)); 
	//CHECK_ALL:     _Array_ptr<int> m =  realloc<int>(w, 2*sizeof(int)); 
    m[1] = 5; 
    z[3] =  2;
} 



