// RUN: cconv-standalone -base-dir=%S -addcr -alltypes -output-postfix=checkedALL %s %S/arrinstructbothmulti2.c
// RUN: cconv-standalone -base-dir=%S -addcr -output-postfix=checkedNOALL %s %S/arrinstructbothmulti2.c
// RUN: %clang -c %S/arrinstructbothmulti1.checkedNOALL.c %S/arrinstructbothmulti2.checkedNOALL.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %S/arrinstructbothmulti1.checkedNOALL.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %S/arrinstructbothmulti1.checkedALL.c %s
// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=checked %S/arrinstructbothmulti2.c %s
// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=convert_again %S/arrinstructbothmulti1.checked.c %S/arrinstructbothmulti2.checked.c
// RUN: diff %S/arrinstructbothmulti1.checked.convert_again.c %S/arrinstructbothmulti1.checked.c
// RUN: diff %S/arrinstructbothmulti2.checked.convert_again.c %S/arrinstructbothmulti2.checked.c
// RUN: rm %S/arrinstructbothmulti1.checkedALL.c %S/arrinstructbothmulti2.checkedALL.c
// RUN: rm %S/arrinstructbothmulti1.checkedNOALL.c %S/arrinstructbothmulti2.checkedNOALL.c
// RUN: rm %S/arrinstructbothmulti1.checked.c %S/arrinstructbothmulti2.checked.c %S/arrinstructbothmulti1.checked.convert_again.c %S/arrinstructbothmulti2.checked.convert_again.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: how the tool behaves when there is an array
field within a struct*/
/*For robustness, this test is identical to arrinstructprotoboth.c and arrinstructboth.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
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

struct warr * sus(struct warr *, struct warr *);
	//CHECK_NOALL: struct warr * sus(struct warr *, struct warr *);
	//CHECK_ALL: _Array_ptr<struct warr> sus(struct warr *, struct warr *y : itype(_Array_ptr<struct warr>));

struct warr * foo() {
	//CHECK_NOALL: struct warr * foo(void) {
	//CHECK_ALL: _Ptr<struct warr> foo(void) {
        struct warr * x = malloc(sizeof(struct warr));
	//CHECK: struct warr * x = malloc<struct warr>(sizeof(struct warr));
        struct warr * y = malloc(sizeof(struct warr));
	//CHECK: struct warr * y = malloc<struct warr>(sizeof(struct warr));
        struct warr * z = sus(x, y);
	//CHECK_NOALL: struct warr * z = sus(x, y);
	//CHECK_ALL: _Ptr<struct warr> z =  sus(x, y);
return z; }

struct warr * bar() {
	//CHECK_NOALL: struct warr * bar(void) {
	//CHECK_ALL: _Ptr<struct warr> bar(void) {
        struct warr * x = malloc(sizeof(struct warr));
	//CHECK: struct warr * x = malloc<struct warr>(sizeof(struct warr));
        struct warr * y = malloc(sizeof(struct warr));
	//CHECK: struct warr * y = malloc<struct warr>(sizeof(struct warr));
        struct warr * z = sus(x, y);
	//CHECK_NOALL: struct warr * z = sus(x, y);
	//CHECK_ALL: _Array_ptr<struct warr> z =  sus(x, y);
z += 2;
return z; }
