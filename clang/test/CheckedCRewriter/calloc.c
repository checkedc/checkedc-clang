// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -


typedef unsigned long size_t;
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);

void func(int *x : itype(_Array_ptr<int>));

void foo(int *w) { 
    int *x = calloc(5, sizeof(int));
    x[2] = 3; 
    func(x);
}
//CHECK_ALL: _Array_ptr<int> x : count(5) =  calloc<int>(5, sizeof(int)); 
//CHECK_NOALL: int *x = calloc<int>(5, sizeof(int));
