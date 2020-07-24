// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -



/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: having a pointer to a pointer*/
/*In this test, foo will treat its return value safely, but sus and bar will not,
through invalid pointer arithmetic, an unsafe cast, etc.*/

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

char *** sus(char * * * x, char * * * y) {
	//CHECK_NOALL: char *** sus(char ***x, _Ptr<_Ptr<_Ptr<char>>> y) {
	//CHECK_ALL: _Array_ptr<_Array_ptr<char *>> sus(char ***x, _Ptr<_Ptr<_Ptr<char>>> y) {
x = (char * * *) 5;
	//CHECK: x = (char * * *) 5;
        char *ch = malloc(sizeof(char)); 
	//CHECK: char *ch = malloc<char>(sizeof(char)); 
        *ch = 'A'; /*Capital A*/
        char *** z = malloc(5*sizeof(char**)); 
	//CHECK_NOALL: char *** z = malloc<char **>(5*sizeof(char**)); 
	//CHECK_ALL: _Array_ptr<_Array_ptr<char *>> z : count(5) =  malloc<_Array_ptr<char *>>(5*sizeof(char**)); 
        for(int i = 0; i < 5; i++) { 
            z[i] = malloc(5*sizeof(char *)); 
	//CHECK: z[i] = malloc<char *>(5*sizeof(char *)); 
            for(int j = 0; j < 5; j++) { 
                z[i][j] = malloc(2*sizeof(char)); 
	//CHECK: z[i][j] = malloc<char>(2*sizeof(char)); 
                strcpy(z[i][j], ch);
                *ch = *ch + 1; 
            }
        }
        
z += 2;
return z; }

char *** foo() {
	//CHECK_NOALL: char *** foo(void) {
	//CHECK_ALL: _Ptr<_Array_ptr<char *>> foo(void) {
        char * * * x = malloc(sizeof(char * *));
	//CHECK: char * * * x = malloc<char **>(sizeof(char * *));
        char * * * y = malloc(sizeof(char * *));
	//CHECK: _Ptr<_Ptr<_Ptr<char>>> y =  malloc<_Ptr<_Ptr<char>>>(sizeof(char * *));
        char *** z = sus(x, y);
	//CHECK_NOALL: char *** z = sus(x, y);
	//CHECK_ALL: _Ptr<_Array_ptr<char *>> z =  sus(x, y);
return z; }

char *** bar() {
	//CHECK_NOALL: char *** bar(void) {
	//CHECK_ALL: _Ptr<_Array_ptr<char *>> bar(void) {
        char * * * x = malloc(sizeof(char * *));
	//CHECK: char * * * x = malloc<char **>(sizeof(char * *));
        char * * * y = malloc(sizeof(char * *));
	//CHECK: _Ptr<_Ptr<_Ptr<char>>> y =  malloc<_Ptr<_Ptr<char>>>(sizeof(char * *));
        char *** z = sus(x, y);
	//CHECK_NOALL: char *** z = sus(x, y);
	//CHECK_ALL: _Array_ptr<_Array_ptr<char *>> z =  sus(x, y);
z += 2;
return z; }
