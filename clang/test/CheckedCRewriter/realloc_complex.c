// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/realloc_complex.checkedNOALL.c
// RUN: rm %S/realloc_complex.checkedNOALL.c

/**********************************************************/
/* This file tests the conversion tool's behavior with    */
/* mulitple complex realloc calls                         */
/**********************************************************/

#define size_t int
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

void foo(int *n) { 
    int *y = malloc(2*sizeof(int)); 
    /*w should be WILD because of unsolvable pointer, arr constraint*/
    int *w = malloc(sizeof(int));
    y[1] = 3;
    int *z = realloc(y, 5*sizeof(int));
    int *m = realloc(w, 2*sizeof(int)); 
    m[1] = 5; 
    z[3] =  2;
} 
//CHECK_ALL: _Array_ptr<int> y: count((2 * sizeof(int))) =  malloc(2*sizeof(int)); 
//CHECK_NOALL: int *y = malloc(2*sizeof(int));
//CHECK: int *w = malloc(sizeof(int));
//CHECK_NOALL: int *z = realloc(y, 5*sizeof(int)); 
//CHECK_NOALL: int *m = realloc(w, 2*sizeof(int));
//CHECK_ALL: _Array_ptr<int> z =  realloc(y, 5*sizeof(int));
//CHECK_ALL: _Array_ptr<int> m =  realloc(w, 2*sizeof(int)); 

