// RUN: rm -rf %t*
// Manually passing fortify source to avoid issues surrounding the way compiler
// builtins are handled. (See issue #630)
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- -D_FORTIFY_SOURCE=0 | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -D_FORTIFY_SOURCE=0 | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- -D_FORTIFY_SOURCE=0 | %clang -c -fcheckedc-extension -x c -o %t1.unused -

// General demonstration
_Itype_for_any(T) void *test_single(void *a
                                    : itype(_Ptr<T>), void *b
                                    : itype(_Ptr<T>))
    : itype(_Ptr<T>);

void t0(int *a, int *b) {
  //CHECK: void t0(_Ptr<int> a, _Ptr<int> b) {
  test_single(a, b);
  //CHECK: test_single<int>(a, b);
}

void t1(int *a, int *b) {
  //CHECK: void t1(int *a : itype(_Ptr<int>), _Ptr<int> b) {
  float c;
  test_single(a, &c);
  //CHECK: test_single(a, &c);
}

void t10(int **a, int **b) {
  //CHECK: void t10(_Ptr<_Ptr<int>> a, _Ptr<_Ptr<int>> b) {
  test_single(a, b);
  //CHECK: test_single<_Ptr<int>>(a, b);
}

_Itype_for_any(T, U) void *test_double(void *a
                                       : itype(_Ptr<T>), void *b
                                       : itype(_Ptr<T>), void *c
                                       : itype(_Ptr<U>), void *d
                                       : itype(_Ptr<U>))
    : itype(_Ptr<T>);

void t2(int *a, int *b, float *c, float *d) {
  //CHECK: void t2(_Ptr<int> a, _Ptr<int> b, _Ptr<float> c, _Ptr<float> d) {
  test_double(a, b, c, d);
  //CHECK: test_double<int,float>(a, b, c, d);
}

void t3(int *a, int *b, float *c, float *d) {
  //CHECK: void t3(int *a : itype(_Ptr<int>), int *b : itype(_Ptr<int>), float *c : itype(_Ptr<float>), float *d : itype(_Ptr<float>)) {
  test_double(a, c, b, d);
  //CHECK: test_double(a, c, b, d);
}

void t4(int *a, int *b, float *c, float *d) {
  //CHECK: void t4(int *a : itype(_Ptr<int>), _Ptr<int> b, _Ptr<float> c, _Ptr<float> d) {
  float e;
  test_double(a, &e, c, d);
  //CHECK: test_double<void,float>(a, &e, c, d);
}

void t5(int *a, int *b, float *c, float *d) {
  //CHECK: void t5(_Ptr<int> a, _Ptr<int> b, float *c : itype(_Ptr<float>), _Ptr<float> d) {
  int e;
  test_double(a, b, c, &e);
  //CHECK: test_double<int,void>(a, b, c, &e);
}

_Itype_for_any(T) void *test_many(void *a
                                  : itype(_Ptr<T>), void *b
                                  : itype(_Ptr<T>), void *c
                                  : itype(_Ptr<T>), void *d
                                  : itype(_Ptr<T>), void *e
                                  : itype(_Ptr<T>))
    : itype(_Ptr<T>);

void t6(int *a, int *b, int *c, int *d, int *e) {
  //CHECK: void t6(_Ptr<int> a, _Ptr<int> b, _Ptr<int> c, _Ptr<int> d, _Ptr<int> e) {
  test_many(a, b, c, d, e);
  //CHECK: test_many<int>(a, b, c, d, e);
}

void t7(int *a, int *b, int *c, int *d, int *e) {
  //CHECK: void t7(int *a : itype(_Ptr<int>), int *b : itype(_Ptr<int>), _Ptr<int> c, int *d : itype(_Ptr<int>), int *e : itype(_Ptr<int>)) {
  float f;
  test_many(a, b, &f, d, e);
  //CHECK: test_many(a, b, &f, d, e);
}

void unsafe(int *a) {
  // CHECK: void unsafe(int *a : itype(_Ptr<int>)) {
  int b = 0;
  test_single(a, b);
  // CHECK: test_single(a, b);
}

// Example issue 153

#include <stdlib.h>
#include <string.h>

void foo(int *p2) {
  int *p = malloc(2 * sizeof(int));
  //CHECK_ALL: _Array_ptr<int> p : count(2) = malloc<int>(2 * sizeof(int));
  memcpy(p, p2, sizeof(int));
  //CHECK: memcpy<int>(p, p2, sizeof(int));
}

// Array types can be used to instantiate type params
void arrs() {
  int *p = malloc(10 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> p : count(10) = malloc<int>(10 * sizeof(int));
  int q[10];
  // CHECK_ALL: int q _Checked[10];

  memcpy(p, q, 10 * sizeof(int));
  // CHECK: memcpy<int>(p, q, 10 * sizeof(int));
}

_Itype_for_any(T) void *test1(void *t : itype(_Ptr<T>)) : itype(_Ptr<T>);
_Itype_for_any(T) void *test2(void) : itype(_Ptr<T>);

void f0() {
  int **a = test2();
  // CHECK: _Ptr<int *> a =  test2<int *>();
  *a = (int *)1;
}

void f1(int **a, float **b) {
  // CHECK: void f1(_Ptr<_Ptr<int>> a, _Ptr<_Ptr<float>> b) {
  int **c = test1(a);
  float **d = test1(b);
  // CHECK: _Ptr<_Ptr<int>> c =  test1<_Ptr<int>>(a);
  // CHECK: _Ptr<_Ptr<float>> d =  test1<_Ptr<float>>(b);
}

void f2(int **a, int **c) {
  // CHECK: void f2(_Ptr<_Ptr<int>> a, int **c : itype(_Ptr<_Ptr<int>>)) {
  int **b = test1(a);
  // CHECK: _Ptr<_Ptr<int>> b =  test1<_Ptr<int>>(a);
  int *d = test1(c);
  // CHECK: int *d = test1(c);
}

void deep(int ****v, int ****w, int ****x, int ****y, int ****z) {
  // CHECK: void deep(int ****v : itype(_Ptr<_Ptr<_Ptr<_Ptr<int>>>>), int ****w : itype(_Ptr<_Ptr<_Ptr<_Ptr<int>>>>), int ****x : itype(_Ptr<_Ptr<_Ptr<_Ptr<int>>>>), int ****y : itype(_Ptr<_Ptr<_Ptr<_Ptr<int>>>>), int ****z : itype(_Ptr<_Ptr<_Ptr<_Ptr<int>>>>)) {

  int ****u = test_many(v, w, x, y, z);
  // CHECK: _Ptr<_Ptr<_Ptr<int *>>> u =  test_many<_Ptr<_Ptr<int *>>>(v, w, x, y, z);
  ***w = (int *)1;

  int ******a = malloc(sizeof(int *****));
  // CHECK: _Ptr<_Ptr<_Ptr<_Ptr<int **>>>> a = malloc<_Ptr<_Ptr<_Ptr<int **>>>>(sizeof(int *****));
  ****a = (int **)1;
}

// Issue #233. Void type paramters were not being detected by
// typeArgsProvidedCheck

// void provided
void *example0(void *ptr, unsigned int size) {
  // CHECK: void *example0(void *ptr, unsigned int size) {
  void *ret;
  ret = realloc<void>(ptr, size);
  // CHECK: ret = realloc<void>(ptr, size);
  return ret;
}

// nothing provided
void *example1(void *ptr, unsigned int size) {
  // CHECK: void *example1(void *ptr, unsigned int size) {
  void *ret;
  ret = realloc(ptr, size);
  // CHECK: ret = realloc<void>(ptr, size);
  return ret;
}

// Issue #349. Check that the parameter doesn't inherit the double pointer
// argument within do_doubleptr
_Itype_for_any(T) void incoming_doubleptr(void *ptr : itype(_Array_ptr<T>)) {
  // CHECK_ALL: _Itype_for_any(T) void incoming_doubleptr(void *ptr : itype(_Array_ptr<T>) count(5)) {
  return;
}

void do_doubleptr(int count) {
  int **arr = malloc(sizeof(int *) * count);
  // CHECK_ALL: _Array_ptr<_Ptr<int>> arr : count(count) = malloc<_Ptr<int>>(sizeof(int *) * count);
  incoming_doubleptr(arr);
  // CHECK_ALL: incoming_doubleptr<_Ptr<int>>(arr);
}

// make sure adding this function doesn't infer
// with the type of the previous one
// Though it does currently add the `count(5)`
// to the param of incomming_doubleptr
void interfere_doubleptr(void) {
  float fl _Checked[5][5] = {};
  incoming_doubleptr(fl);
}
