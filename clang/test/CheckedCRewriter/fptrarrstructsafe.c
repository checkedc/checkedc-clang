// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" %s
//RUN: cconv-standalone -output-postfix=checkedNOALL %s
//RUN: %clang -c %S/fptrarrstructsafe.checkedNOALL.c
//RUN: rm %S/fptrarrstructsafe.checkedNOALL.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: using a function pointer and an array as fields
of a struct that interact with each other*/
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
//CHECK_NOALL:     int *values; 
//CHECK_NOALL:     char *name;
//CHECK_NOALL:     _Ptr<int (int )> mapper;

//CHECK_ALL:     _Array_ptr<int> values : count(5); 
//CHECK_ALL:     char *name;
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

struct fptrarr * sus(struct fptrarr *x, struct fptrarr *y) {
 
        x = (struct fptrarr *) 5; 
        char name[30]; 
        struct fptrarr *z = malloc(sizeof(struct fptrarr)); 
        z->values = y->values; 
        z->name = strcpy(name, "Hello World");
        z->mapper = fact; 
        int i;
        for(i = 0; i < 5; i++) { 
            z->values[i] = z->mapper(z->values[i]);
        }
        
return z; }
//CHECK_NOALL: _Ptr<struct fptrarr> sus(struct fptrarr *x, _Ptr<struct fptrarr> y) {
//CHECK_NOALL:         _Ptr<struct fptrarr> z =  malloc<struct fptrarr>(sizeof(struct fptrarr)); 
//CHECK_ALL: _Ptr<struct fptrarr> sus(struct fptrarr *x, _Ptr<struct fptrarr> y) {
//CHECK_ALL:         _Ptr<struct fptrarr> z =  malloc<struct fptrarr>(sizeof(struct fptrarr)); 

struct fptrarr * foo() {
 
        char name[20]; 
        struct fptrarr * x = malloc(sizeof(struct fptrarr));
        struct fptrarr *y =  malloc(sizeof(struct fptrarr));
        int *yvals = calloc(5, sizeof(int)); 
        int i;
        for(i = 0; i < 5; i++) {
            yvals[i] = i+1; 
            }  
        y->values = yvals; 
        y->name = name; 
        y->mapper = NULL;
        strcpy(y->name, "Example"); 
        struct fptrarr *z = sus(x, y);
        
return z; }
//CHECK_NOALL: _Ptr<struct fptrarr> foo(void) {
//CHECK_NOALL:         struct fptrarr * x = malloc<struct fptrarr>(sizeof(struct fptrarr));
//CHECK_NOALL:         _Ptr<struct fptrarr> y =   malloc<struct fptrarr>(sizeof(struct fptrarr));
//CHECK_NOALL:         int *yvals = calloc<int>(5, sizeof(int)); 
//CHECK_NOALL:         strcpy(y->name, ((const char *)"Example")); 
//CHECK_NOALL:         _Ptr<struct fptrarr> z =  sus(x, y);
//CHECK_ALL: _Ptr<struct fptrarr> foo(void) {
//CHECK_ALL:         struct fptrarr * x = malloc<struct fptrarr>(sizeof(struct fptrarr));
//CHECK_ALL:         _Ptr<struct fptrarr> y =   malloc<struct fptrarr>(sizeof(struct fptrarr));
//CHECK_ALL:         _Array_ptr<int> yvals : count(5) =  calloc<int>(5, sizeof(int)); 
//CHECK_ALL:         strcpy(y->name, ((const char *)"Example")); 
//CHECK_ALL:         _Ptr<struct fptrarr> z =  sus(x, y);

struct fptrarr * bar() {
 
        char name[20]; 
        struct fptrarr * x = malloc(sizeof(struct fptrarr));
        struct fptrarr *y =  malloc(sizeof(struct fptrarr));
        int *yvals = calloc(5, sizeof(int)); 
        int i;
        for(i = 0; i < 5; i++) {
            yvals[i] = i+1; 
            }  
        y->values = yvals; 
        y->name = name; 
        y->mapper = NULL;
        strcpy(y->name, "Example"); 
        struct fptrarr *z = sus(x, y);
        
return z; }
//CHECK_NOALL: _Ptr<struct fptrarr> bar(void) {
//CHECK_NOALL:         struct fptrarr * x = malloc<struct fptrarr>(sizeof(struct fptrarr));
//CHECK_NOALL:         _Ptr<struct fptrarr> y =   malloc<struct fptrarr>(sizeof(struct fptrarr));
//CHECK_NOALL:         int *yvals = calloc<int>(5, sizeof(int)); 
//CHECK_NOALL:         strcpy(y->name, ((const char *)"Example")); 
//CHECK_NOALL:         _Ptr<struct fptrarr> z =  sus(x, y);
//CHECK_ALL: _Ptr<struct fptrarr> bar(void) {
//CHECK_ALL:         struct fptrarr * x = malloc<struct fptrarr>(sizeof(struct fptrarr));
//CHECK_ALL:         _Ptr<struct fptrarr> y =   malloc<struct fptrarr>(sizeof(struct fptrarr));
//CHECK_ALL:         _Array_ptr<int> yvals : count(5) =  calloc<int>(5, sizeof(int)); 
//CHECK_ALL:         strcpy(y->name, ((const char *)"Example")); 
//CHECK_ALL:         _Ptr<struct fptrarr> z =  sus(x, y);
