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

//CHECK:     int *values; 

//CHECK:     _Ptr<int (int )> mapper;

//CHECK:     _Ptr<int (_Ptr<int> )> func;
//CHECK:     int args _Checked[5]; 
//CHECK:     _Ptr<int (int )> funcs _Checked[5];

struct fptrarr * sus(struct fptrarr *x, struct fptrarr *y) {
 
        x = (struct fptrarr *) 5; 
        char name[30]; 
        struct fptrarr *z = malloc(sizeof(struct fptrarr)); 
        z->values = y->values; 
        z->name = strcpy(name, "Hello World");
        z->mapper = fact; 
        for(int i = 0; i < 5; i++) { 
            z->values[i] = z->mapper(z->values[i]);
        }
        
z += 2;
return z; }
//CHECK: struct fptrarr * sus(struct fptrarr *x, struct fptrarr *y : itype(_Ptr<struct fptrarr>)) {

struct fptrarr * foo() {
 
        char name[20]; 
        struct fptrarr * x = malloc(sizeof(struct fptrarr));
        struct fptrarr *y =  malloc(sizeof(struct fptrarr));
        int *yvals = calloc(5, sizeof(int)); 
        for(int i = 0; i < 5; i++) {
            yvals[i] = i+1; 
            }  
        y->values = yvals; 
        y->name = name; 
        y->mapper = NULL;
        strcpy(y->name, "Example"); 
        struct fptrarr *z = sus(x, y);
        
return z; }
//CHECK: struct fptrarr * foo() {

struct fptrarr * bar() {
 
        char name[20]; 
        struct fptrarr * x = malloc(sizeof(struct fptrarr));
        struct fptrarr *y =  malloc(sizeof(struct fptrarr));
        int *yvals = calloc(5, sizeof(int)); 
        for(int i = 0; i < 5; i++) {
            yvals[i] = i+1; 
            }  
        y->values = yvals; 
        y->name = name; 
        y->mapper = NULL;
        strcpy(y->name, "Example"); 
        struct fptrarr *z = sus(x, y);
        
z += 2;
return z; }
//CHECK: struct fptrarr * bar() {
