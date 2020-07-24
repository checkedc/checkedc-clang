// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" %s
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: cconv-standalone -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -


// Examples from issue #58

void f(int *p) {
  int *x = (int *)p;
  // CHECK_NOALL: _Ptr<int> x = (_Ptr<int> )p;
  // CHECK_ALL: _Ptr<int> x = (_Ptr<int> )p;
}

void g(int p[]) {
  int *x = (int *)p;
  // CHECK_NOALL: int *x = (int *)p;
  // CHECK_All: _Ptr<int> x = (_Ptr<int> )p;
}

// A very similar issue with function pointers

int add1(int a){
  return a + 1;
}

void h() {
  int (*x)(int) = add1;
  // CHECK_NOALL: _Ptr<int (int )> x = add1;
  // CHECK_ALL: _Ptr<int (int )> x = add1;
}

void i() {
  int (*x)(int) = (int(*)(int))add1;
  // CHECK_NOALL: _Ptr<int (int )> x = (_Ptr<int (int )> )add1;
  // CHECK_ALL: _Ptr<int (int )> x = (_Ptr<int (int )> )add1;
}
