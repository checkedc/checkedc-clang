// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/calloc.checked.c -- | count 0
// RUN: rm %S/calloc.checked.c


typedef unsigned long size_t;
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);

void func(int *x : itype(_Array_ptr<int>));
	//CHECK: void func(int *x : itype(_Array_ptr<int>));

void foo(int *w) { 
	//CHECK: void foo(_Ptr<int> w) { 
    int *x = calloc(5, sizeof(int));
	//CHECK_NOALL: int *x = calloc<int>(5, sizeof(int));
	//CHECK_ALL:     _Array_ptr<int> x : count(5) =  calloc<int>(5, sizeof(int));
    x[2] = 3; 
    func(x);
}
