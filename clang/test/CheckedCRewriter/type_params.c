// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// General demonstration
_Itype_for_any(T) void *test_single(void *a : itype(_Ptr<T>), void *b : itype(_Ptr<T>)) : itype(_Ptr<T>);

void t0(int *a, int *b){
//CHECK: void t0(_Ptr<int> a, _Ptr<int> b){
  test_single(a, b);
  //CHECK: test_single<int>(a, b);
}

void t1(int *a, int *b){
//CHECK: void t1(int *a, _Ptr<int> b){
  float c;
  test_single(a, &c);
  //CHECK: test_single(a, &c);
}

// BUG: This does not compile when converted because the correct checked type
// is not inserted for the type argument. Not fixing here because this is
// resolved in PR #187.
void t10(int **a, int **b) {
//COM: void t10(_Ptr<_Ptr<int>> a, _Ptr<_Ptr<int>> b) {
  // test_single(a, b);
  //COM: test_single<int *>(a, b);
}

_Itype_for_any(T,U) void *test_double(void *a : itype(_Ptr<T>), void *b : itype(_Ptr<T>), void *c : itype(_Ptr<U>), void *d : itype(_Ptr<U>)) : itype(_Ptr<T>);

void t2(int *a, int *b, float *c, float *d) {
//CHECK: void t2(_Ptr<int> a, _Ptr<int> b, _Ptr<float> c, _Ptr<float> d) {
  test_double(a, b, c, d);
  //CHECK: test_double<int,float>(a, b, c, d);
}

void t3(int *a, int *b, float *c, float *d) {
//CHECK: void t3(int *a, int *b, float *c, float *d) {
  test_double(a, c, b, d);
  //CHECK: test_double(a, c, b, d);
}

void t4(int *a, int *b, float *c, float *d) {
//CHECK: void t4(int *a, _Ptr<int> b, _Ptr<float> c, _Ptr<float> d) {
  float e;
  test_double(a, &e, c, d);
  //CHECK: test_double<void,float>(a, &e, c, d);
}

void t5(int *a, int *b, float *c, float *d) {
//CHECK: void t5(_Ptr<int> a, _Ptr<int> b, float *c, _Ptr<float> d) {
  int e;
  test_double(a, b, c, &e);
  //CHECK: test_double<int,void>(a, b, c, &e);
}

_Itype_for_any(T) void *test_many(void *a : itype(_Ptr<T>), void *b : itype(_Ptr<T>), void *c : itype(_Ptr<T>), void *d : itype(_Ptr<T>), void *e : itype(_Ptr<T>)) : itype(_Ptr<T>);

void t6(int *a, int *b, int *c, int *d, int *e) {
//CHECK: void t6(_Ptr<int> a, _Ptr<int> b, _Ptr<int> c, _Ptr<int> d, _Ptr<int> e) {
  test_many(a, b, c, d, e);
  //CHECK: test_many<int>(a, b, c, d, e);
}

void t7(int *a, int *b, int *c, int *d, int *e) {
//CHECK: void t7(int *a, int *b, _Ptr<int> c, int *d, int *e) {
  float f;
  test_many(a, b, &f, d, e);
  //CHECK: test_many(a, b, &f, d, e);
}

// Example issue 153

#include <stddef.h>
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
_Itype_for_any(T) void *memcpy(void * restrict dest : itype(restrict _Array_ptr<T>) byte_count(n),
             const void * restrict src : itype(restrict _Array_ptr<const T>) byte_count(n),
             size_t n) : itype(_Array_ptr<T>) byte_count(n);

void foo(int *p2) {
    int *p = malloc(2*sizeof(int));
    //CHECK_ALL: _Array_ptr<int> p =  malloc<int>(2*sizeof(int));
    memcpy(p, p2, sizeof(int));
    //CHECK: memcpy<int>(p, p2, sizeof(int));
}
