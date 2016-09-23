// Tests for Checked C rewriter tool.
//
// Checks properties of functions.
//
// RUN: checked-c-convert %s -- | FileCheck %s
// RUN: checked-c-convert %s -- | %clang_cc1 -verify -fcheckedc-extension -x c -
// expected-no-diagnostics

// Have something so that we always get some output.
void a0(void) {
  int q = 0;
  int *k = &q;
  *k = 0;
}
//CHECK: int q = 0;
//CHECK-NEXT: _Ptr<int> k = &q;

void mut(int *);

void a1(void) {
  int a = 0;
  int *b = &a;

  mut(b);
}
//CHECK: int a = 0;
//CHECK-NEXT: int *b = &a;

// Test function pointer assignment and constraint propagation. 

void *xyzzy(int *a, int b) {
  *a = b;

  return 0;
}
//CHECK: void *xyzzy(_Ptr<int> a, int b) {
//CHECK-NEXT: *a = b;

void xyzzy_driver(void) {
  void *(*xyzzy_ptr)(int*, int) = &xyzzy;
  int u = 0;
  int *v = &u;
  xyzzy_ptr(v, u);
}
//CHECK: void xyzzy_driver(void) {
//CHECK-NEXT: _Ptr<void* (_Ptr<int> , int )> xyzzy_ptr = &xyzzy;
//CHECK-NEXT: int u = 0;
//CHECK-NEXT: _Ptr<int> v = &u;
//CHECK-NEXT: xyzzy_ptr(v, u);

void bad_mut(int *a, int b, int c) {
  *(a+b) = c;
}
//CHECK: void bad_mut(int *a, int b, int c) {
//CHECK-NEXT: *(a+b) = c;

void bad_mut_driver(void) {
  void (*bad_mut_ptr)(int *, int, int) = &bad_mut;
  int a = 0;
  int *b = &a;
  bad_mut_ptr(b, 2, 0);
}
//CHECK: void bad_mut_driver(void) {
//CHECK-NEXT: _Ptr<void (int* , int , int )> bad_mut_ptr  = &bad_mut;
//CHECK-NEXT: int a = 0;
//CHECK-NEXT: int *b = &a;
//CHECK-NEXT: bad_mut_ptr(b, 2, 0);

// Test function-like macros.
#define SWAP(T, a, b) {\
  T tmp; \
  tmp = a; \
  a = b; \
  b = tmp; }

void SWAPdriver(void) {
  int swapa = 1;
  int swapb = 2;
  int *pswapc = &swapa; 
  int *pswapd = &swapb; 
  SWAP(int*, pswapc, pswapd); 
}
//CHECK: void SWAPdriver(void) {
//CHECK-NEXT: int swapa = 1;
//CHECK-NEXT: int swapb = 2;
//CHECK-NEXT: int *pswapc = &swapa;
//CHECK-NEXT: int *pswapd = &swapb;
//CHECK-NEXT: SWAP(int*, pswapc, pswapd);

// Test vararg externs.
int varargxyzzy(int a, ...);

void varargxyzzy_driver(void) {
  char a[10];
  char *b = &a[0];
  char *c = &a[0];
  *c = 0;
  varargxyzzy(1, b);
}
//CHECK: void varargxyzzy_driver(void) {
//CHECK-NEXT: char a[10];
//CHECK-NEXT: char *b = &a[0];
//CHECK-NEXT: _Ptr<char> c = &a[0];
//CHECK-NEXT: *c = 0;
//CHECK-NEXT: varargxyzzy(1, b);

// Test externs declared in macros.
#define NFUN(rty, nm1, nm2, pty1, pnm1, pty2, pnm2) \
  rty nm1(pty1 pnm1, pty2 pnm2); \
  rty nm2(pty1 pnm1, pty2 pnm2);

NFUN(int*, xyzzyfoo, xyzzybar, int*, bla1, int*, bla2);

void nfundriver(void) {
  int a = 0;
  int *b = &a; 
  int *c = &a; 
  int *d = &a; 
  int *e = &a; 

  xyzzyfoo(b, c); 
  xyzzybar(d, e); 
}
//CHECK: void nfundriver(void) {
//CHECK-NEXT: int a = 0;
//CHECK-NEXT: int *b = &a;
//CHECK-NEXT: int *c = &a;
//CHECK-NEXT: int *d = &a;
//CHECK-NEXT: int *e = &a;

// A function pointer stored in a record where the function pointer returns
// a value which could be a _Ptr. 
int *ok_mut(int *a, int b) {
  *a = b;
  return a;
}
//CHECK: _Ptr<int> ok_mut(_Ptr<int> a, int b) {

typedef struct _B {
  int *(*foo)(int *, int);
} B, *PB;
//CHECK: typedef struct _B {
//CHECK-NEXT: _Ptr<_Ptr<int> (_Ptr<int> , int )> foo ;
//CHECK-NEXT: } B, *PB;

void bdriver(void) {
  B b = {0};
  b.foo = &ok_mut;
  int a = 0;
  int *c = &a;
  int *d = b.foo(c, 0);
}
//CHECK: void bdriver(void) {
//CHECK-NEXT: B b = {0};
//CHECK-NEXT: b.foo = &ok_mut;
//CHECK-NEXT: int a = 0;
//CHECK-NEXT: _Ptr<int> c = &a;
//CHECK-NEXT: _Ptr<int> d = b.foo(c, 0);

// Function pointers returned aren't currently supported.

// Function which returns a typedefed function pointer
typedef int *(*ok_mut_t)(int*,int);

int *ok_mut_clone(int *a, int b) {
  *a = b;
  return a;
}
//CHECK: int *ok_mut_clone(int *a, int b) {

ok_mut_t get_mut_2(void) {
  return &ok_mut_clone;
}
//CHECK: ok_mut_t get_mut_2(void) {

// Function which returns a function pointer
int *(*get_mut(void))(int*,int) {
  return &ok_mut_clone;
}
//CHECK: int *(*get_mut(void))(int*,int) {
