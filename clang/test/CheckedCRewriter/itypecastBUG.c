// Tests for Checked C rewriter tool.
//
// Checks cast insertion while passing arguments to itype parameters.
//
// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked %s 
// RUN: cconv-standalone %S/itypecast.checked.c -- | diff -w %S/itypecast.checked.c -
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
