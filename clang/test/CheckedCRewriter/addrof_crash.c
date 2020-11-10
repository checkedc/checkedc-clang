// RUN: cconv-standalone -alltypes %s -- | FileCheck %s
// RUN: cconv-standalone -alltypes %s -- | %clang_cc1  -fno-builtin -verify -fcheckedc-extension -x c -

// No conversions expected for these two, they just shouldn't crash

extern void foo(int* a : itype(_Array_ptr<int>));
void bar(void) {
  int a = 1;
  foo(&a);
}

extern void foo2(const void *__optval : itype(_Array_ptr<const void>) byte_count(__optlen), unsigned int __optlen);
void bar2(void) {
  int reuseaddr = 1;
  foo2(&reuseaddr, sizeof(reuseaddr));
}

void test(int* a){
//CHECK: void test(int *a : itype(_Array_ptr<int>)){
  ++a;
}
void test2() {
  int a = 1;
  int *b = &a;
  //CHECK: _Ptr<int> b = &a;
  test(&a);
}

void test3(){
  int a, b;

  int *c[1] = {&a};
  // CHECK: _Ptr<int> c _Checked[1] =  {&a};
  int *d[1] = {&b};
  // CHECK: _Ptr<int> d _Checked[1] =  {&b};

  int **e = &((0?c:d)[0]); // expected-error {{expression has unknown bounds, cast to ptr<T> expects source to have bounds}}
  // CHECK: _Ptr<_Ptr<int>> e =  &((0?c:d)[0]);
}

void test4() {
  int *x = (int *)5;
  // CHECK: int *x = (int *)5;
  int *p = &(*x);
  // CHECK: int *p = &(*x);
}
