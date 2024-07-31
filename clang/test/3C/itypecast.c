// Tests for 3C.
//
// Checks cast insertion while passing arguments to itype parameters.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -Wno-error=int-conversion | %clang -c -Wno-error=int-conversion -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked %s -- -Wno-error=int-conversion
// RUN: 3c -base-dir=%t.checked %t.checked/itypecast.c -- -Wno-error=int-conversion | diff -w %t.checked/itypecast.c -

int foo(int **p : itype(_Ptr<_Ptr<int>>));
int bar(int **p : itype(_Ptr<int *>));
int baz(int((*compar)(const int *, const int *))
        : itype(_Ptr<int(_Ptr<const int>, _Ptr<const int>)>));

int func(void) {
  int((*fptr1)(const int *, const int *));
  int((*fptr2)(const int *, const int *));
  //CHECK: _Ptr<int (_Ptr<const int>, _Ptr<const int>)> fptr1 = ((void *)0);
  //CHECK-NEXT: _Ptr<int (_Ptr<const int>, _Ptr<const int>)> fptr2 = ((void *)0);

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
//CHECK-NEXT: fptr1(_Assume_bounds_cast<_Ptr<const int>>(2), 0);
//CHECK-NEXT: baz(fptr1);
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
