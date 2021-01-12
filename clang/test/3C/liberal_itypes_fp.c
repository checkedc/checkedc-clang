// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -alltypes -output-postfix=checked %s
// RUN: 3c -alltypes %S/liberal_itypes_fp.checked.c -- | count 0
// RUN: rm %S/liberal_itypes_fp.checked.c

void fp_test0(int *i) { i = 0; }
// CHECK: void fp_test0(int *i : itype(_Ptr<int>)) { i = 0; }

void fp_test1(int *i) { i = 1; }
// CHECK: void fp_test1(int *i : itype(_Ptr<int>)) { i = 1; }

void fp_caller() {
  void (*j)(int *);
  // CHECK: _Ptr<void (int * : itype(_Ptr<int>))> j = ((void *)0);
  if (0) {
    j = fp_test0;
  } else {
    j = fp_test1;
  }

  int *k;
  j(k);
  // CHECK: _Ptr<int> k = ((void *)0);
  // CHECK: j(k);

  int *l = 1;
  j(l);
  // CHECK: int *l = 1;
  // CHECK: j(l);
}

void fp_test2(int *i) { }
// CHECK: void fp_test2(_Ptr<int> i) _Checked { }
void fp_test3(int *i) { }
// CHECK: void fp_test3(_Ptr<int> i) _Checked { }

void fp_unsafe_caller() {
  void (*a)(int *);
  // CHECK: _Ptr<void (_Ptr<int> )> a = ((void *)0);
  if (0) {
    a = fp_test2;
  } else {
    a = fp_test3;
  }

  int *b = 1;
  // CHECK: int *b = 1;
  a(b);
  // CHECK: a(_Assume_bounds_cast<_Ptr<int>>(b));

  int *c;
  // CHECK: _Ptr<int> c = ((void *)0);
  a(c);
  // CHECK: a(c);
}

int *fp_test4() { return 0; }
// CHECK: int *fp_test4(void) : itype(_Ptr<int>) _Checked { return 0; }
int *fp_test5() { return 1; }
// CHECK: int *fp_test5(void) : itype(_Ptr<int>) _Checked { return 1; }

void fp_unsafe_return() {
  int *(*j)();
  // CHECK: _Ptr<int *(void) : itype(_Ptr<int>)> j = ((void *)0);
  if (0) {
    j = fp_test4;
  } else {
    j = fp_test5;
  }

  int *k = j();
  // CHECK: _Ptr<int> k = j();
}

void f_ptr_arg(int (*f)()) {
// CHECK: void f_ptr_arg(int (*f)()) {
  f = 1;
}

void fpnc0(void (*fptr)(void *)) { }
// CHECK: void fpnc0(_Ptr<void (void *)> fptr) { }
void fpnc1(void* p1) {}
// CHECK: void fpnc1(void* p1) {}
void fpnc2() { fpnc0(fpnc1); }
// CHECK: void fpnc2() { fpnc0(fpnc1); }
void fpnc3(void (*fptr)(void *)) { fptr = 1; }
// CHECK: void fpnc3(void (*fptr)(void *)) { fptr = 1; }
void fpnc4(void* p1) {}
// CHECK: void fpnc4(void* p1) {}
void fpnc5() { fpnc3(fpnc4); }
// CHECK: void fpnc5() { fpnc3(fpnc4); }

int fptr_itype(void ((*f)(int *)) : itype(_Ptr<void (_Ptr<int>)>));
//CHECK: int fptr_itype(void ((*f)(int *)) : itype(_Ptr<void (_Ptr<int>)>));

void fptr_itype_test(void) {
    _Ptr<int (_Ptr<int>)> fptr1 = ((void *)0);
    //CHECK: _Ptr<int (_Ptr<int>)> fptr1 = ((void *)0);
    baz(fptr1);
    //CHECK: baz(fptr1);

    int (*fptr2)(int *);
    //CHECK: _Ptr<int (_Ptr<int> )> fptr2 = ((void *)0);
    baz(fptr2);
    //CHECK: baz(fptr2);
}
