// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" %s
//RUN: cconv-standalone -output-postfix=checkedNOALL %s
//RUN: %clang -c %S/fptrsafeprotosafe.checkedNOALL.c
//RUN: rm %S/fptrsafeprotosafe.checkedNOALL.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: converting the callee into a function pointer
and then using that pointer for computations*/
/*For robustness, this test is identical to fptrsafesafe.c except in that
a prototype for sus is available, and is called by foo and bar,
while the definition for sus appears below them*/
/*In this test, foo, bar, and sus will all treat their return values safely*/

/*********************************************************************************/


#define size_t int
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
};
//CHECK_NOALL:     _Ptr<struct general> next;

//CHECK_ALL:     _Ptr<struct general> next;


struct warr { 
    int data1[5];
    char *name;
};
//CHECK_NOALL:     int data1[5];
//CHECK_NOALL:     _Ptr<char> name;

//CHECK_ALL:     int data1 _Checked[5];
//CHECK_ALL:     _Ptr<char> name;


struct fptrarr { 
    int *values; 
    char *name;
    int (*mapper)(int);
};
//CHECK_NOALL:     _Ptr<int> values; 
//CHECK_NOALL:     _Ptr<char> name;
//CHECK_NOALL:     _Ptr<int (int )> mapper;

//CHECK_ALL:     _Ptr<int> values; 
//CHECK_ALL:     _Ptr<char> name;
//CHECK_ALL:     _Ptr<int (int )> mapper;


struct fptr { 
    int *value; 
    int (*func)(int);
};  
//CHECK_NOALL:     _Ptr<int> value; 
//CHECK_NOALL:     _Ptr<int (int )> func;

//CHECK_ALL:     _Ptr<int> value; 
//CHECK_ALL:     _Ptr<int (int )> func;


struct arrfptr { 
    int args[5]; 
    int (*funcs[5]) (int);
};
//CHECK_NOALL:     int args[5]; 
//CHECK_NOALL:     int (*funcs[5]) (int);

//CHECK_ALL:     int args _Checked[5]; 
//CHECK_ALL:     _Ptr<int (int )> funcs _Checked[5];


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
    *x *= 2; 
    return x;
}

//CHECK_NOALL: _Ptr<int> mul2(_Ptr<int> x) { 

//CHECK_ALL: _Ptr<int> mul2(_Ptr<int> x) { 

int * sus(struct general *, struct general *);
//CHECK_NOALL: int * sus(struct general *x, _Ptr<struct general> y);
//CHECK_ALL: _Nt_array_ptr<int> sus(struct general *x, _Ptr<struct general> y);

int * foo() {

        struct general *x = malloc(sizeof(struct general)); 
        struct general *y = malloc(sizeof(struct general));
        struct general *curr = y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int * (*sus_ptr)(struct general *, struct general *) = sus;   
        int *z = sus_ptr(x, y);
        
return z; }
//CHECK_NOALL: int * foo() {
//CHECK_NOALL:         struct general *x = malloc<struct general>(sizeof(struct general)); 
//CHECK_NOALL:         _Ptr<struct general> y =  malloc<struct general>(sizeof(struct general));
//CHECK_NOALL:         _Ptr<struct general> curr =  y;
//CHECK_NOALL:         _Ptr<int* (struct general *, _Ptr<struct general> )> sus_ptr =  sus;   
//CHECK_NOALL:         int *z = sus_ptr(x, y);
//CHECK_ALL: _Nt_array_ptr<int> foo(void) {
//CHECK_ALL:         struct general *x = malloc<struct general>(sizeof(struct general)); 
//CHECK_ALL:         _Ptr<struct general> y =  malloc<struct general>(sizeof(struct general));
//CHECK_ALL:         _Ptr<struct general> curr =  y;
//CHECK_ALL:         _Ptr<_Nt_array_ptr<int> (struct general *, _Ptr<struct general> )> sus_ptr =  sus;   
//CHECK_ALL:         _Nt_array_ptr<int> z =  sus_ptr(x, y);

int * bar() {

        struct general *x = malloc(sizeof(struct general)); 
        struct general *y = malloc(sizeof(struct general));
        struct general *curr = y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int * (*sus_ptr)(struct general *, struct general *) = sus;   
        int *z = sus_ptr(x, y);
        
return z; }
//CHECK_NOALL: int * bar() {
//CHECK_NOALL:         struct general *x = malloc<struct general>(sizeof(struct general)); 
//CHECK_NOALL:         _Ptr<struct general> y =  malloc<struct general>(sizeof(struct general));
//CHECK_NOALL:         _Ptr<struct general> curr =  y;
//CHECK_NOALL:         _Ptr<int* (struct general *, _Ptr<struct general> )> sus_ptr =  sus;   
//CHECK_NOALL:         int *z = sus_ptr(x, y);
//CHECK_ALL: _Nt_array_ptr<int> bar(void) {
//CHECK_ALL:         struct general *x = malloc<struct general>(sizeof(struct general)); 
//CHECK_ALL:         _Ptr<struct general> y =  malloc<struct general>(sizeof(struct general));
//CHECK_ALL:         _Ptr<struct general> curr =  y;
//CHECK_ALL:         _Ptr<_Nt_array_ptr<int> (struct general *, _Ptr<struct general> )> sus_ptr =  sus;   
//CHECK_ALL:         _Nt_array_ptr<int> z =  sus_ptr(x, y);

int * sus(struct general *x, struct general *y) {

        x = (struct general *) 5;
        int *z = calloc(5, sizeof(int)); 
        struct general *p = y;
        int i;
        for(i = 0; i < 5; p = p->next, i++) { 
            z[i] = p->data; 
        } 
        
return z; }
//CHECK_NOALL: int * sus(struct general *x, _Ptr<struct general> y) {
//CHECK_NOALL:         int *z = calloc<int>(5, sizeof(int)); 
//CHECK_NOALL:         _Ptr<struct general> p =  y;
//CHECK_ALL: _Nt_array_ptr<int> sus(struct general *x, _Ptr<struct general> y) {
//CHECK_ALL:         _Nt_array_ptr<int> z : count(5) =  calloc<int>(5, sizeof(int)); 
//CHECK_ALL:         _Ptr<struct general> p =  y;
