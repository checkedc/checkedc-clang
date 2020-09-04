// RUN: cconv-standalone -base-dir=%S -addcr -alltypes -output-postfix=checkedALL %s %S/fptrsafecalleemulti2.c
// RUN: cconv-standalone -base-dir=%S -addcr -output-postfix=checkedNOALL %s %S/fptrsafecalleemulti2.c
// RUN: %clang -c %S/fptrsafecalleemulti1.checkedNOALL.c %S/fptrsafecalleemulti2.checkedNOALL.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %S/fptrsafecalleemulti1.checkedNOALL.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %S/fptrsafecalleemulti1.checkedALL.c %s
// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=checked %S/fptrsafecalleemulti2.c %s
// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=convert_again %S/fptrsafecalleemulti1.checked.c %S/fptrsafecalleemulti2.checked.c
// RUN: diff %S/fptrsafecalleemulti1.checked.convert_again.c %S/fptrsafecalleemulti1.checked.c
// RUN: diff %S/fptrsafecalleemulti2.checked.convert_again.c %S/fptrsafecalleemulti2.checked.c
// RUN: rm %S/fptrsafecalleemulti1.checkedALL.c %S/fptrsafecalleemulti2.checkedALL.c
// RUN: rm %S/fptrsafecalleemulti1.checkedNOALL.c %S/fptrsafecalleemulti2.checkedNOALL.c
// RUN: rm %S/fptrsafecalleemulti1.checked.c %S/fptrsafecalleemulti2.checked.c %S/fptrsafecalleemulti1.checked.convert_again.c %S/fptrsafecalleemulti2.checked.convert_again.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: converting the callee into a function pointer
and then using that pointer for computations*/
/*For robustness, this test is identical to fptrsafeprotocallee.c and fptrsafecallee.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
/*In this test, foo and bar will treat their return values safely, but sus will
not, through invalid pointer arithmetic, an unsafe cast, etc*/

/*********************************************************************************/


#include <stddef.h>
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

int * sus(struct general *, struct general *);
	//CHECK_NOALL: int * sus(struct general *, _Ptr<struct general> y);
	//CHECK_ALL: _Array_ptr<int> sus(struct general *, _Ptr<struct general> y);

int * foo() {
	//CHECK_NOALL: int * foo(void) {
	//CHECK_ALL: _Array_ptr<int> foo(void) {

        struct general *x = malloc(sizeof(struct general)); 
	//CHECK: struct general *x = malloc<struct general>(sizeof(struct general)); 
        struct general *y = malloc(sizeof(struct general));
	//CHECK: _Ptr<struct general> y = malloc<struct general>(sizeof(struct general));
        struct general *curr = y;
	//CHECK: _Ptr<struct general> curr = y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int * (*sus_ptr)(struct general *, struct general *) = sus;   
	//CHECK_NOALL: _Ptr<int * (struct general *, _Ptr<struct general> )> sus_ptr = sus;   
	//CHECK_ALL: _Ptr<_Array_ptr<int> (struct general *, _Ptr<struct general> )> sus_ptr = sus;   
        int *z = sus_ptr(x, y);
	//CHECK_NOALL: int *z = sus_ptr(x, y);
	//CHECK_ALL: _Array_ptr<int> z = sus_ptr(x, y);
        
return z; }

int * bar() {
	//CHECK_NOALL: int * bar(void) {
	//CHECK_ALL: _Array_ptr<int> bar(void) {

        struct general *x = malloc(sizeof(struct general)); 
	//CHECK: struct general *x = malloc<struct general>(sizeof(struct general)); 
        struct general *y = malloc(sizeof(struct general));
	//CHECK: _Ptr<struct general> y = malloc<struct general>(sizeof(struct general));
        struct general *curr = y;
	//CHECK: _Ptr<struct general> curr = y;
        int i;
        for(i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int * (*sus_ptr)(struct general *, struct general *) = sus;   
	//CHECK_NOALL: _Ptr<int * (struct general *, _Ptr<struct general> )> sus_ptr = sus;   
	//CHECK_ALL: _Ptr<_Array_ptr<int> (struct general *, _Ptr<struct general> )> sus_ptr = sus;   
        int *z = sus_ptr(x, y);
	//CHECK_NOALL: int *z = sus_ptr(x, y);
	//CHECK_ALL: _Array_ptr<int> z = sus_ptr(x, y);
        
return z; }
