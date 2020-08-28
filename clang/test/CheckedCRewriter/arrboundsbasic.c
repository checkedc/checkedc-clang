// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s


/*
Basic array bounds tests (without any data-flow analysis).
*/


#include <stddef.h>

extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);


struct bar {
    char *a;
    unsigned a_len;
};
//CHECK: _Array_ptr<char> a : count(a_len);

// Here, the bounds of arr will be count(len)
int foo(int *arr, unsigned len) {
    unsigned i, n;
    struct bar a;
    char *arr1 = malloc(n*sizeof(char));
    char *arr2 = calloc(n, sizeof(char));
    arr1++;
    arr2++;
    for (i=0; i<len; i++) {
        arr[i] = 0;
    }
    a.a_len = 4;
    a.a = malloc(a.a_len*sizeof(char));
    a.a[0] = 0;
    return 0;
}
//CHECK: int foo(_Array_ptr<int> arr : count(len), unsigned int len) {
//CHECK: _Array_ptr<char> arr1 : count(n) = malloc<char>(n*sizeof(char));
//CHECK: _Array_ptr<char> arr2 : count(n) =  calloc<char>(n, sizeof(char));
