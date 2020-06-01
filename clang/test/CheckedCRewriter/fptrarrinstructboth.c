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

struct arrfptr * sus(struct arrfptr *x, struct arrfptr *y) {
 
        x = (struct arrfptr *) 5; 
        struct arrfptr *z = malloc(sizeof(struct arrfptr)); 
        for(int i = 0; i < 5; i++) { 
            z->args[i] = i + 1; 
        } 
        z->funcs[0] = add1;
        z->funcs[1] = sub1; 
        z->funcs[2] = zerohuh;
        z->funcs[3] = fib;
        z->funcs[4] = fact;
        
z += 2;
return z; }
//CHECK: struct arrfptr * sus(struct arrfptr *x, struct arrfptr *y : itype(_Ptr<struct arrfptr>)) {

struct arrfptr * foo() {
 
        struct arrfptr * x = malloc(sizeof(struct arrfptr));
        struct arrfptr * y =  malloc(sizeof(struct arrfptr));
       
        struct arrfptr *z = sus(x, y); 
        for(int i = 0; i < 5; i++) { 
            z->args[i] = z->funcs[i](z->args[i]);
        }
        
return z; }
//CHECK: struct arrfptr * foo() {

struct arrfptr * bar() {
 
        struct arrfptr * x = malloc(sizeof(struct arrfptr));
        struct arrfptr * y =  malloc(sizeof(struct arrfptr));
       
        struct arrfptr *z = sus(x, y); 
        for(int i = 0; i < 5; i++) { 
            z->args[i] = z->funcs[i](z->args[i]);
        }
        
z += 2;
return z; }
//CHECK: struct arrfptr * bar() {
