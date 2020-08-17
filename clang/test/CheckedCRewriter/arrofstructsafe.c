// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// RUN: cconv-standalone -alltypes -output-postfix=checked %s
// RUN: cconv-standalone -alltypes %S/arrofstructsafe.checked.c -- | diff %S/arrofstructsafe.checked.c -
// RUN: rm %S/arrofstructsafe.checked.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: how the tool behaves when there is an array
of structs*/
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
	//CHECK_NOALL: struct general *next;
	//CHECK_ALL: _Ptr<struct general> next;
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

struct general ** sus(struct general * x, struct general * y) {
	//CHECK_NOALL: struct general ** sus(struct general * x, struct general * y) {
	//CHECK_ALL: _Array_ptr<_Ptr<struct general>> sus(struct general *x, _Ptr<struct general> y) {
x = (struct general *) 5; 
	//CHECK: x = (struct general *) 5; 
        struct general **z = calloc(5, sizeof(struct general *));
	//CHECK_NOALL: struct general **z = calloc<struct general *>(5, sizeof(struct general *));
	//CHECK_ALL: _Array_ptr<_Ptr<struct general>> z : count(5) =  calloc<_Ptr<struct general>>(5, sizeof(struct general *));
        struct general *curr = y;
	//CHECK_NOALL: struct general *curr = y;
	//CHECK_ALL: _Ptr<struct general> curr =  y;
        int i;
        for(i = 0; i < 5; i++) { 
	//CHECK_NOALL: for(i = 0; i < 5; i++) { 
	//CHECK_ALL: for(i = 0; i < 5; i++) _Checked { 
            z[i] = curr; 
            curr = curr->next; 
        } 
        
return z; }

struct general ** foo() {
	//CHECK_NOALL: struct general ** foo(void) {
	//CHECK_ALL: _Array_ptr<_Ptr<struct general>> foo(void) {
        struct general * x = malloc(sizeof(struct general));
	//CHECK: struct general * x = malloc<struct general>(sizeof(struct general));
        struct general * y = malloc(sizeof(struct general));
	//CHECK_NOALL: struct general * y = malloc<struct general>(sizeof(struct general));
	//CHECK_ALL: _Ptr<struct general> y =  malloc<struct general>(sizeof(struct general));
        
        struct general *curr = y;
	//CHECK_NOALL: struct general *curr = y;
	//CHECK_ALL: _Ptr<struct general> curr =  y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        struct general ** z = sus(x, y);
	//CHECK_NOALL: struct general ** z = sus(x, y);
	//CHECK_ALL: _Array_ptr<_Ptr<struct general>> z =  sus(x, y);
return z; }

struct general ** bar() {
	//CHECK_NOALL: struct general ** bar(void) {
	//CHECK_ALL: _Array_ptr<_Ptr<struct general>> bar(void) {
        struct general * x = malloc(sizeof(struct general));
	//CHECK: struct general * x = malloc<struct general>(sizeof(struct general));
        struct general * y = malloc(sizeof(struct general));
	//CHECK_NOALL: struct general * y = malloc<struct general>(sizeof(struct general));
	//CHECK_ALL: _Ptr<struct general> y =  malloc<struct general>(sizeof(struct general));
        
        struct general *curr = y;
	//CHECK_NOALL: struct general *curr = y;
	//CHECK_ALL: _Ptr<struct general> curr =  y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        struct general ** z = sus(x, y);
	//CHECK_NOALL: struct general ** z = sus(x, y);
	//CHECK_ALL: _Array_ptr<_Ptr<struct general>> z =  sus(x, y);
return z; }
