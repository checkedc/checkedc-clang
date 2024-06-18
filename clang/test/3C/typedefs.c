// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S --addcr --alltypes %s -- | %clang -c  -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S --addcr %s -- | %clang -c  -fcheckedc-extension -x c -o /dev/null -
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

typedef int *integer;
//CHECK: typedef _Ptr<int> integer;
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
  typedef int nat;
  const nat z = 3;
  const nat *cnstp = &z;
  //CHECK: _Ptr<const nat> cnstp = &z;
  int w = 34;
  const integer c = &w;
  //CHECK: const integer c = &w;

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

#define MYDECL typedef int *ZZZ;
MYDECL
void zzz(void) {
  int x = 3;
  ZZZ z = &x;
}
typedef int **const a;
//CHECK: typedef const _Ptr<_Ptr<int>> a;
void xxx(void) { a b; }
