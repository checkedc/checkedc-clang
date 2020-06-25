// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/realloc_reusediff.checkedNOALL.c
// RUN: rm %S/realloc_reusediff.checkedNOALL.c

/**********************************************************/
/* This file tests the conversion tool's behavior with    */
/* realloc calls where the same pointer type is redefined.*/
/**********************************************************/


#define size_t int
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

/*  x should be marked as wild because it's being used 
    differently, thereby generating an unsolvable constraint. */

void foo(int *count) { 
    /*using it as a pointer*/
    int *x = malloc(sizeof(int));  
    *x = 3;
    /*now using it as an array*/
    x = realloc(x, sizeof(int)*(*count));
    x[2] = 2;
}

//CHECK: int *x = malloc(sizeof(int));