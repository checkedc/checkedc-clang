// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone --addcr --alltypes %s -- | %clang_cc1  -fcheckedc-extension -x c -
// RUN: cconv-standalone --addcr %s -- | %clang_cc1  -fcheckedc-extension -x c -
//
// cconv-standalone -alltypes -output-postfix=checked %s
// cconv-standalone -alltypes %S/typedefs.checked.c -- | diff %S/typedefs.checked.c -
// rm %S/typedefs.checked.c

typedef int* intptr;
//CHECK: typedef _Ptr<int> intptr;

typedef intptr* PP;
//CHECK: typedef _Ptr<intptr> PP;

typedef int* bad;
//CHECK: typedef int* bad;

typedef intptr* badP;
//CHECK: typedef intptr* badP;

typedef struct A { 
    int* x;
    int z;
    PP p;
} A;
//CHECK: _Ptr<int> x;
//CHECK: PP p;

intptr bar(intptr x) { 
	//CHECK: intptr bar(intptr x) _Checked {
	return x;
}

int foo(void) { 
    //CHECK: int foo(void) {
    int x = 3;
    intptr p = &x;
    //CHECK: intptr p = &x;
    PP pp = &p;
    //CHECK: PP pp = &p;
    A a = { &x, 3, pp };
    //CHECK: A a = { &x, 3, pp };
    bad b = (int*) 3;
    //CHECK: bad b = (int*) 3;
    badP b2 = (intptr*) 3;

    return *p;
}
