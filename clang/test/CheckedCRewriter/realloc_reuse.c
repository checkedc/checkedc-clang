// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/realloc_reuse.checkedNOALL.c
// RUN: rm %S/realloc_reuse.checkedNOALL.c

/**********************************************************/
/* This file tests the conversion tool's behavior with    */
/* realloc calls where the same pointer type is redefined.*/
/**********************************************************/


#define size_t int
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

void foo(int *count) { 
    int *x = malloc(2*sizeof(int));  
    x[1] = 4; 
    x = realloc(x, sizeof(int)*(*count));
    x[2] = 2;
}

//CHECK_NOALL: int *x = malloc(2*sizeof(int));
//CHECK_ALL: _Array_ptr<int> x: count((2 * sizeof(int))) =  malloc(2*sizeof(int));