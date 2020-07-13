// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s

int *a() {
// CHECK: _Array_ptr<int> a(void) {
  int *a = 0;
  // CHECK: _Array_ptr<int> a =  0;
  return a++;
}

int *dumb(int *a){
// CHECK: _Ptr<int> dumb(_Ptr<int> a){
  int *b = a;
  // CHECK: _Ptr<int> b =  a;
  return b;
}

int *f(void) {
// CHECK: _Array_ptr<int> f(void) {
  int *p = (int*)0;
  // CHECK: _Array_ptr<int> p =  (int*)0;
  p++;
  return p;
}

int *foo(void) {
// CHECK: _Array_ptr<int> foo(void) {
  int *q = f();
  // CHECK: _Array_ptr<int> q =  f();
  return q;
}

#define size_t int
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);

int *bar() {
// CHECK: _Nt_array_ptr<int> bar(void) {
  int *z = calloc(2, sizeof(int));
  //CHECK: _Nt_array_ptr<int> z : count(2) =  calloc<int>(2, sizeof(int));
  z += 2;
  return z;
}

int *baz(int *a) {
  // CHECK: _Array_ptr<int> baz(_Array_ptr<int> a) {
  a++;

  int *b = (int*) 0;
  // CHECK: _Array_ptr<int> b = (int*) 0;
  a = b;

  int *c = b;
  // CHECK: _Array_ptr<int> c = b;

  return c;
}

int *buz(int *a) {
  // CHECK: _Ptr<int> buz(_Array_ptr<int> a) {
  a++;

  int *b = (int*) 0;
  // CHECK: _Array_ptr<int> b = (int*) 0;
  a = b;

  // The current implementation does not propagate array constraint to c and d, but
  // if this test starts failing because it does, that's probably OK.

  int *c = b;
  // CHECK: _Ptr<int>  c = b;

  int *d = (int*) 0;
  // CHECK: _Ptr<int> d = (int*) 0;
  c = d;

  return d;
}
