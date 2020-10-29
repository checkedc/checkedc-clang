// Tests for Checked C rewriter tool.
//
// Checks cast insertion while passing arguments to itype parameters.
//
// RUN: 3c -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked %s 
// RUN: 3c %S/itypecast.checked.c -- | diff -w %S/itypecast.checked.c -
// RUN: rm %S/itypecast.checked.c

int foo(int **p:itype(_Ptr<_Ptr<int>>));
int bar(int **p:itype(_Ptr<int *>));
int baz(int ((*compar)(const int *, const int *)) :
             itype(_Ptr<int (_Ptr<const int>, _Ptr<const int>)>));
             
int func(void) {
    int ((*fptr1)(const int *, const int *));
    int ((*fptr2)(const int *, const int *));
    //CHECK: _Ptr<int (const int *, _Ptr<const int> )> fptr1 = ((void *)0);
    //CHECK-NEXT: _Ptr<int (_Ptr<const int> , _Ptr<const int> )> fptr2 = ((void *)0);

    int **fp1;
    int **fp2;
    int **bp1;
    int **bp2;
    //CHECK: _Ptr<_Ptr<int>> fp1 = ((void *)0);
    //CHECK-NEXT: _Ptr<int *> fp2 = ((void *)0);
    //CHECK-NEXT: _Ptr<int *> bp1 = ((void *)0);
    //CHECK-NEXT: int **bp2;

    *fp2 = 2;
    foo(fp1);
    foo(fp2);
    bar(bp1);
    bp2 = 2;
    *bp2 = 2;
    bar(bp2);
    fptr1(2, 0);
    baz(fptr1);
    baz(fptr2);
    return 0;    
}
//CHECK: foo(fp1);
//CHECK-NEXT: foo(((int **)fp2));
//CHECK-NEXT: bar(bp1);
//CHECK: bar(bp2);
//CHECK-NEXT: fptr1(2, 0);
//CHECK-NEXT: baz(((int ((*)(const int *, const int *)) )fptr1));
//CHECK-NEXT: baz(fptr2);

int func2(void) {
    int **fp1;
    // CHECK: _Ptr<int *> fp1 = ((void *)0);
    *fp1 = 1;
    foo(fp1);
    // CHECK: foo(((int **)fp1));
}

int func3(void) {
    _Ptr<int *> fp1 = ((void *)0);
    // CHECK: _Ptr<int *> fp1 = ((void *)0);
    *fp1 = 1;
    foo(((int **)fp1));
    // CHECK: foo(((int **)fp1));
}
