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
  //CHECK: _Nt_array_ptr<int> z =  calloc(2, sizeof(int));
  z += 2;
  return z;
}
