// RUN: cconv-standalone -base-dir=%S -output-postfix=checked %s %S/ptrTOptrcallermulti2.c
//RUN: FileCheck -match-full-lines --input-file %S/ptrTOptrcallermulti1.checked.c %s
//RUN: rm %S/ptrTOptrcallermulti1.checked.c %S/ptrTOptrcallermulti2.checked.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: having a pointer to a pointer*/
/*For robustness, this test is identical to ptrTOptrprotocaller.c and ptrTOptrcaller.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
/*In this test, foo and sus will treat their return values safely, but bar will
not, through invalid pointer arithmetic, an unsafe cast, etc.*/

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
//CHECK:     _Ptr<struct general> next;


struct warr { 
    int data1[5];
    char *name;
};
//CHECK:     int data1[5];
//CHECK-NEXT:     _Ptr<char> name;


struct fptrarr { 
    int *values; 
    char *name;
    int (*mapper)(int);
};
//CHECK:     _Ptr<int> values; 
//CHECK-NEXT:     _Ptr<char> name;
//CHECK-NEXT:     _Ptr<int (int )> mapper;


struct fptr { 
    int *value; 
    int (*func)(int);
};  
//CHECK:     _Ptr<int> value; 
//CHECK-NEXT:     _Ptr<int (int )> func;


struct arrfptr { 
    int args[5]; 
    int (*funcs[5]) (int);
};
//CHECK:     int args[5]; 
//CHECK-NEXT:     int (*funcs[5]) (int);


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

//CHECK: _Ptr<int> mul2(_Ptr<int> x) { 

char *** sus(char * * *, char * * *);
//CHECK: char *** sus(char * * *, char * * *);

char *** foo() {
        char * * * x = malloc(sizeof(char * *));
        char * * * y = malloc(sizeof(char * *));
        char *** z = sus(x, y);
return z; }
//CHECK: char *** foo() {
//CHECK:         char * * * x = malloc(sizeof(char * *));
//CHECK:         char * * * y = malloc(sizeof(char * *));
//CHECK:         char *** z = sus(x, y);

char *** bar() {
        char * * * x = malloc(sizeof(char * *));
        char * * * y = malloc(sizeof(char * *));
        char *** z = sus(x, y);
z += 2;
return z; }
//CHECK: char *** bar() {
//CHECK:         char * * * x = malloc(sizeof(char * *));
//CHECK:         char * * * y = malloc(sizeof(char * *));
//CHECK:         char *** z = sus(x, y);
