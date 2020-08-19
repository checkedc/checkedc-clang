// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null - 

typedef unsigned long size_t;
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

void foo2(int *x) { 
    int *p;
    //CHECK_ALL: _Array_ptr<int> p : count(3) = ((void *)0);
    //CHECK_NOALL: int *p;
    p = ({int *q = malloc(3*sizeof(int)); q[2] = 1; q;});
    //CHECK_ALL: p = ({_Array_ptr<int> q : count(3) =  malloc<int>(3*sizeof(int)); q[2] = 1; q;});
    //CHECK_NOALL: p = ({int *q = malloc<int>(3*sizeof(int)); q[2] = 1; q;});
    p[1] = 3;
} 