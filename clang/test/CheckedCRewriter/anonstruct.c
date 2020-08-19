// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/anonstruct.checked.c -- | count 0
// RUN: rm %S/anonstruct.checked.c

struct { 
	/*the fields of the anonymous struct are free to be marked checked*/
    int *data; 
	//CHECK_NOALL: int *data; 
	//CHECK_ALL: _Array_ptr<int> data : count(4); 

/* but the actual pointer can't be */
} *x; 

/*ensure trivial conversion*/
void foo(int *w) { 
	//CHECK: void foo(_Ptr<int> w) { 
	x->data = malloc(sizeof(int)*4); 
	x->data[1] = 4;
} 

