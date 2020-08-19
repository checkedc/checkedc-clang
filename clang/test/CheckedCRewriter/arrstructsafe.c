// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// RUN: cconv-standalone -alltypes -output-postfix=checked %s
// RUN: cconv-standalone -alltypes %S/arrstructsafe.checked.c -- | diff %S/arrstructsafe.checked.c -
// RUN: rm %S/arrstructsafe.checked.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: arrays and structs, specifically by using an array to
traverse through the values of a struct*/
/*In this test, foo, bar, and sus will all treat their return values safely*/

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

int * sus(struct general * x, struct general * y) {
	//CHECK_NOALL: int * sus(struct general *x, _Ptr<struct general> y) {
	//CHECK_ALL: _Array_ptr<int> sus(struct general *x, _Ptr<struct general> y) {
x = (struct general *) 5;
	//CHECK: x = (struct general *) 5;
        int *z = calloc(5, sizeof(int)); 
	//CHECK_NOALL: int *z = calloc<int>(5, sizeof(int)); 
	//CHECK_ALL: _Array_ptr<int> z : count(5) =  calloc<int>(5, sizeof(int)); 
        struct general *p = y;
	//CHECK: _Ptr<struct general> p =  y;
        int i;
        for(i = 0; i < 5; p = p->next, i++) { 
	//CHECK_NOALL: for(i = 0; i < 5; p = p->next, i++) { 
	//CHECK_ALL: for(i = 0; i < 5; p = p->next, i++) _Checked { 
            z[i] = p->data; 
        } 
        
return z; }

int * foo() {
	//CHECK_NOALL: int * foo(void) {
	//CHECK_ALL: _Array_ptr<int> foo(void) {
        struct general * x = malloc(sizeof(struct general));
	//CHECK: struct general * x = malloc<struct general>(sizeof(struct general));
        struct general * y = malloc(sizeof(struct general));
	//CHECK: _Ptr<struct general> y =  malloc<struct general>(sizeof(struct general));
        
        struct general *curr = y;
	//CHECK: _Ptr<struct general> curr =  y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int * z = sus(x, y);
	//CHECK_NOALL: int * z = sus(x, y);
	//CHECK_ALL: _Array_ptr<int> z =  sus(x, y);
return z; }

int * bar() {
	//CHECK_NOALL: int * bar(void) {
	//CHECK_ALL: _Array_ptr<int> bar(void) {
        struct general * x = malloc(sizeof(struct general));
	//CHECK: struct general * x = malloc<struct general>(sizeof(struct general));
        struct general * y = malloc(sizeof(struct general));
	//CHECK: _Ptr<struct general> y =  malloc<struct general>(sizeof(struct general));
        
        struct general *curr = y;
	//CHECK: _Ptr<struct general> curr =  y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int * z = sus(x, y);
	//CHECK_NOALL: int * z = sus(x, y);
	//CHECK_ALL: _Array_ptr<int> z =  sus(x, y);
return z; }
