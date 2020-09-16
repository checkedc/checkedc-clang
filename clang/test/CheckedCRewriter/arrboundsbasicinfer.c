// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s


/*
Basic array bounds tests (without any data-flow analysis).
*/


#include <stddef.h>

extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);


struct bar {
    char *a;
    unsigned h;
    unsigned b;
};

//CHECK: _Array_ptr<char> a : count(b);

int foo(int *arr, unsigned len) {
    unsigned i =0;
    for (i=0; i<len; i++) {
        arr[i] = 0;
    }
    return 0;
}

//CHECK: int foo(_Array_ptr<int> arr : count(len), unsigned int len) {

void baz() {
    unsigned n;
    struct bar c;
    int *arr1;
    foo(arr1, n);
    c.a = malloc(c.b*sizeof(char));
    c.a[0] = 0;
}

//CHECK: _Array_ptr<int> arr1 : count(n) = ((void *)0);
