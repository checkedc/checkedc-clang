// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -


#define size_t int
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);

void foo(int *w) { 
    /*only allocating 1 thing, so should be converted even without alltypes*/
    int *x = calloc(1, sizeof(int));
    *x = 5; 

    /*allocating multiple things, should only be converted when alltypes is on*/
    int *y = calloc(5, sizeof(int)); 
}
//CHECK: _Ptr<int> x = calloc<int>(1, sizeof(int));
//CHECK_ALL: _Ptr<int> y = calloc<int>(5, sizeof(int)); 
//CHECK_NOALL: int *y = calloc<int>(5, sizeof(int));
