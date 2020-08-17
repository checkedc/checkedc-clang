// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// RUN: cconv-standalone -alltypes -output-postfix=checked %s
// RUN: cconv-standalone -alltypes %S/fptrinstructprotocallee.checked.c -- | diff %S/fptrinstructprotocallee.checked.c -
// RUN: rm %S/fptrinstructprotocallee.checked.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: how the tool behaves when a function pointer
is a field of a struct*/
/*For robustness, this test is identical to fptrinstructcallee.c except in that
a prototype for sus is available, and is called by foo and bar,
while the definition for sus appears below them*/
/*In this test, foo and bar will treat their return values safely, but sus will
not, through invalid pointer arithmetic, an unsafe cast, etc*/

/*********************************************************************************/


typedef unsigned long size_t;
#define NULL 0
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
	//CHECK: int add1(int x) _Checked { 
    return x+1;
} 

int sub1(int x) { 
	//CHECK: int sub1(int x) _Checked { 
    return x-1; 
} 

int fact(int n) { 
	//CHECK: int fact(int n) _Checked { 
    if(n==0) { 
        return 1;
    } 
    return n*fact(n-1);
} 

int fib(int n) { 
	//CHECK: int fib(int n) _Checked { 
    if(n==0) { return 0; } 
    if(n==1) { return 1; } 
    return fib(n-1) + fib(n-2);
} 

int zerohuh(int n) { 
	//CHECK: int zerohuh(int n) _Checked { 
    return !n;
}

int *mul2(int *x) { 
	//CHECK: _Ptr<int> mul2(_Ptr<int> x) _Checked { 
    *x *= 2; 
    return x;
}

struct fptr * sus(struct fptr *, struct fptr *);
	//CHECK: struct fptr * sus(struct fptr *x, _Ptr<struct fptr> y);

struct fptr * foo() {
	//CHECK: struct fptr * foo(void) {
 
        struct fptr * x = malloc(sizeof(struct fptr)); 
	//CHECK: struct fptr * x = malloc<struct fptr>(sizeof(struct fptr)); 
        struct fptr *y =  malloc(sizeof(struct fptr));
	//CHECK: _Ptr<struct fptr> y =   malloc<struct fptr>(sizeof(struct fptr));
        struct fptr *z = sus(x, y);
	//CHECK: struct fptr *z = sus(x, y);
        
return z; }

struct fptr * bar() {
	//CHECK: struct fptr * bar(void) {
 
        struct fptr * x = malloc(sizeof(struct fptr)); 
	//CHECK: struct fptr * x = malloc<struct fptr>(sizeof(struct fptr)); 
        struct fptr *y =  malloc(sizeof(struct fptr));
	//CHECK: _Ptr<struct fptr> y =   malloc<struct fptr>(sizeof(struct fptr));
        struct fptr *z = sus(x, y);
	//CHECK: struct fptr *z = sus(x, y);
        
return z; }

struct fptr * sus(struct fptr *x, struct fptr *y) {
	//CHECK: struct fptr * sus(struct fptr *x, _Ptr<struct fptr> y) {
 
        x = (struct fptr *) 5; 
	//CHECK: x = (struct fptr *) 5; 
        struct fptr *z = malloc(sizeof(struct fptr)); 
	//CHECK: struct fptr *z = malloc<struct fptr>(sizeof(struct fptr)); 
        z->value = y->value; 
        z->func = fact;
        
z += 2;
return z; }
