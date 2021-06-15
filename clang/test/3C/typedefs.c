// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S --addcr --alltypes %s -- | %clang_cc1  -fcheckedc-extension -x c -
// RUN: 3c -base-dir=%S --addcr %s -- | %clang_cc1  -fcheckedc-extension -x c -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/typedefs.c -- | diff %t.checked/typedefs.c -

typedef int *intptr;
//CHECK: typedef _Ptr<int> intptr;

typedef intptr *PP;
//CHECK: typedef _Ptr<intptr> PP;

typedef int *bad;
//CHECK: typedef int *bad;

typedef intptr *badP;
//CHECK: typedef intptr *badP;

typedef struct A {
  int *x;
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
  A a = {&x, 3, pp};
  //CHECK: A a = {&x, 3, pp};
  bad b = (int *)3;
  //CHECK: bad b = (int *)3;
  badP b2 = (intptr *)3;

  return *p;
}

typedef int *startswild;
//CHECK: typedef _Ptr<int> startswild;
typedef _Ptr<startswild> startschecked;
//CHECK: typedef _Ptr<startswild> startschecked;

void foobar(void) {
  int x = 3;
  startswild p = &x;
  startschecked pp = &p;
}

typedef int *intptr;
//CHECK: typedef _Ptr<int> intptr;
void barfoo(intptr x) {
  //CHECK: void barfoo(intptr x) _Checked {
  *x = 5;
}
