// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
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

struct warr { 
    int data1[5];
    char name[];
};

struct fptrarr { 
    int *values; 
    char *name;
    int (*mapper)(int);
};

struct fptr { 
    int *value; 
    int (*func)(int*);
};  

struct arrfptr { 
    int args[5]; 
    int (*funcs[5]) (int);
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
    *x *= 2; 
    return x;
}
//CHECK:     _Ptr<struct general> next;

//CHECK:     int data1 _Checked[5];
//CHECK:     _Ptr<char> name;

//CHECK:     _Ptr<int> values; 

//CHECK:     _Ptr<int (int )> mapper;

//CHECK:     _Ptr<int (_Ptr<int> )> func;
//CHECK:     int args _Checked[5]; 
//CHECK:     _Ptr<int (int )> funcs _Checked[5];

int ** sus(int *, int *);
//CHECK: int ** sus(int *x, int *y : itype(_Array_ptr<int>));

int ** foo() {

        int *x = malloc(sizeof(int)); 
        int *y = calloc(5, sizeof(int)); 
        for(int i = 0; i < 5; i++) { 
            y[i] = i+1;
        } 
        int *z = sus(x, y);
        
return z; }
//CHECK: int ** foo() {

int ** bar() {

        int *x = malloc(sizeof(int)); 
        int *y = calloc(5, sizeof(int)); 
        for(int i = 0; i < 5; i++) { 
            y[i] = i+1;
        } 
        int *z = sus(x, y);
        
return z; }
//CHECK: int ** bar() {

int ** sus(int *x, int *y) {

        x = (int *) 5;
        int **z = calloc(5, sizeof(int *)); 
        int * (*mul2ptr) (int *) = mul2;
        for(int i = 0; i < 5; i++) { 
            z[i] = mul2ptr(&y[i]);
        } 
        
z += 2;
return z; }
//CHECK: int ** sus(int *x, int *y : itype(_Array_ptr<int>)) {
