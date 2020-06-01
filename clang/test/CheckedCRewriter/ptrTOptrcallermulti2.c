// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=checked2 %s %S/ptrTOptrcallermulti1.c
//RUN: FileCheck -match-full-lines --input-file %S/ptrTOptrcallermulti2.checked2.c %s
//RUN: rm %S/ptrTOptrcallermulti1.checked2.c %S/ptrTOptrcallermulti2.checked2.c
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

char *** sus(char * * * x, char * * * y) {
x = (char * * *) 5;
        char *ch = malloc(sizeof(char)); 
        *ch = 'A'; /*Capital A*/
        char *** z = malloc(5*sizeof(char**)); 
        for(int i = 0; i < 5; i++) { 
            z[i] = malloc(5*sizeof(char *)); 
            for(int j = 0; j < 5; j++) { 
                z[i][j] = malloc(2*sizeof(char)); 
                strcpy(z[i][j], ch);
                *ch = *ch + 1; 
            }
        }
        
return z; }
//CHECK: char *** sus(char ***x, char ***y : itype(_Ptr<char**>)) {
