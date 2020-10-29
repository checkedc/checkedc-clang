// RUN: cconv-standalone -base-dir=%S -addcr -alltypes -output-postfix=checkedALL2 %S/ptrTOptrbothmulti1.c %s
// RUN: cconv-standalone -base-dir=%S -addcr -output-postfix=checkedNOALL2 %S/ptrTOptrbothmulti1.c %s
// RUN: %clang -c %S/ptrTOptrbothmulti1.checkedNOALL2.c %S/ptrTOptrbothmulti2.checkedNOALL2.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %S/ptrTOptrbothmulti2.checkedNOALL2.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %S/ptrTOptrbothmulti2.checkedALL2.c %s
// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=checked2 %S/ptrTOptrbothmulti1.c %s
// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=convert_again %S/ptrTOptrbothmulti1.checked2.c %S/ptrTOptrbothmulti2.checked2.c
// RUN: test ! -f %S/ptrTOptrbothmulti1.checked2.convert_again.c
// RUN: test ! -f %S/ptrTOptrbothmulti2.checked2.convert_again.c
// RUN: rm %S/ptrTOptrbothmulti1.checkedALL2.c %S/ptrTOptrbothmulti2.checkedALL2.c
// RUN: rm %S/ptrTOptrbothmulti1.checkedNOALL2.c %S/ptrTOptrbothmulti2.checkedNOALL2.c
// RUN: rm %S/ptrTOptrbothmulti1.checked2.c %S/ptrTOptrbothmulti2.checked2.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: having a pointer to a pointer*/
/*For robustness, this test is identical to ptrTOptrprotoboth.c and ptrTOptrboth.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
/*In this test, foo will treat its return value safely, but sus and bar will not,
through invalid pointer arithmetic, an unsafe cast, etc.*/

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

char *** sus(char * * * x, char * * * y) {
	//CHECK_NOALL: char *** sus(char * * * x, _Ptr<_Ptr<_Ptr<char>>> y) {
	//CHECK_ALL: _Array_ptr<_Array_ptr<char *>> sus(char * * * x, _Ptr<_Ptr<_Ptr<char>>> y) {
x = (char * * *) 5;
	//CHECK: x = (char * * *) 5;
        char *ch = malloc(sizeof(char)); 
	//CHECK: char *ch = malloc<char>(sizeof(char)); 
        *ch = 'A'; /*Capital A*/
        char *** z = malloc(5*sizeof(char**)); 
	//CHECK_NOALL: char *** z = malloc<char **>(5*sizeof(char**)); 
	//CHECK_ALL: _Array_ptr<_Array_ptr<char *>> z = malloc<_Array_ptr<char *>>(5*sizeof(char**)); 
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
