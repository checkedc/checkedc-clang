// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -



/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: arrays through a for loop and pointer
arithmetic to assign into it*/
/*In this test, foo and bar will treat their return values safely, but sus will
not, through invalid pointer arithmetic, an unsafe cast, etc*/

/*********************************************************************************/


#include <stddef.h>
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

struct general { 
    int data; 
    struct general *next;
	//CHECK: _Ptr<struct general> next;
};

struct warr { 
    int data1[5];
	//CHECK_NOALL: int data1[5];
	//CHECK_ALL: int data1 _Checked[5];
    char *name;
	//CHECK: _Ptr<char> name;
};

struct fptrarr { 
    int *values; 
	//CHECK: _Ptr<int> values; 
    char *name;
	//CHECK: _Ptr<char> name;
    int (*mapper)(int);
	//CHECK: _Ptr<int (int )> mapper;
};

struct fptr { 
    int *value; 
	//CHECK: _Ptr<int> value; 
    int (*func)(int);
	//CHECK: _Ptr<int (int )> func;
};  

struct arrfptr { 
    int args[5]; 
	//CHECK_NOALL: int args[5]; 
	//CHECK_ALL: int args _Checked[5]; 
    int (*funcs[5]) (int);
	//CHECK_NOALL: int (*funcs[5]) (int);
	//CHECK_ALL: _Ptr<int (int )> funcs _Checked[5];
};

int add1(int x) { 
    return x+1;
} 

int sub1(int x) { 
    return x-1; 
} 

int fact(int n) { 
    if(n==0) { 
        return 1;
    } 
    return n*fact(n-1);
} 

int fib(int n) { 
    if(n==0) { return 0; } 
    if(n==1) { return 1; } 
    return fib(n-1) + fib(n-2);
} 

int zerohuh(int n) { 
    return !n;
}

int *mul2(int *x) { 
	//CHECK: _Ptr<int> mul2(_Ptr<int> x) { 
    *x *= 2; 
    return x;
}

int * sus(int * x, int * y) {
	//CHECK_NOALL: int * sus(int *x, _Ptr<int> y) {
	//CHECK_ALL: _Nt_array_ptr<int> sus(int *x, _Ptr<int> y) {
x = (int *) 5;
	//CHECK: x = (int *) 5;
        int *z = calloc(5, sizeof(int)); 
	//CHECK_NOALL: int *z = calloc<int>(5, sizeof(int)); 
	//CHECK_ALL: _Nt_array_ptr<int> z : count(5) =  calloc<int>(5, sizeof(int)); 
        int i, fac;
        int *p;
	//CHECK_NOALL: int *p;
	//CHECK_ALL: _Array_ptr<int> p : count(5) = ((void *)0);
        for(i = 0, p = z, fac = 1; i < 5; ++i, p++, fac *= i) 
        { *p = fac; }
z += 2;
return z; }

int * foo() {
	//CHECK_NOALL: int * foo(void) {
	//CHECK_ALL: _Nt_array_ptr<int> foo(void) {
        int * x = malloc(sizeof(int));
	//CHECK: int * x = malloc<int>(sizeof(int));
        int * y = malloc(sizeof(int));
	//CHECK: _Ptr<int> y =  malloc<int>(sizeof(int));
        int * z = sus(x, y);
	//CHECK_NOALL: int * z = sus(x, y);
	//CHECK_ALL: _Nt_array_ptr<int> z =  sus(x, y);
return z; }

int * bar() {
	//CHECK_NOALL: int * bar(void) {
	//CHECK_ALL: _Nt_array_ptr<int> bar(void) {
        int * x = malloc(sizeof(int));
	//CHECK: int * x = malloc<int>(sizeof(int));
        int * y = malloc(sizeof(int));
	//CHECK: _Ptr<int> y =  malloc<int>(sizeof(int));
        int * z = sus(x, y);
	//CHECK_NOALL: int * z = sus(x, y);
	//CHECK_ALL: _Nt_array_ptr<int> z =  sus(x, y);
return z; }
