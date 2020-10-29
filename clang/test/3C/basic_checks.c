// Tests for Checked C rewriter tool.
//
// Tests properties about type re-writing and replacement, and simple function
// return value stuff.
//
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK","CHECK_NEXT" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK","CHECK-NEXT" %s
// RUN: cconv-standalone %s -- | %clang_cc1  -verify -fcheckedc-extension -x c -
// expected-no-diagnostics
//

typedef unsigned short wchar_t;

void typd_driver(void) {
  wchar_t buf[10];
  wchar_t *a = &buf[0];
  wchar_t *b = &buf[0];

  *a = 0;

  *(b+4) = 0;
}
//CHECK: void typd_driver(void) {
//CHECK_NOALL: wchar_t buf[10];
//CHECK_NOALL: wchar_t *a = &buf[0];
//CHECK_NOALL: wchar_t *b = &buf[0];

//CHECK_ALL: wchar_t buf _Checked[10];
//CHECK_ALL: _Ptr<wchar_t> a = &buf[0];
//CHECK_ALL: _Array_ptr<wchar_t> b : count(10) = &buf[0];


typedef struct _A {
  int a;
  int b;
} A, *PA;

void mut_pa(PA p) {
  p->a = 0;
  p->b = 1;
}
//CHECK: void mut_pa(_Ptr<struct _A> p) {

void pa_driver(void) {
  A a = {0};
  PA b = &a;

  mut_pa(b);
}
//CHECK: void pa_driver(void) {
//CHECK-NEXT: A a = {0};
//CHECK-NEXT: _Ptr<struct _A> b = &a;

int *id(int *a) {
  return a;
}
//CHECK: _Ptr<int> id(_Ptr<int> a) {

extern int *fry(void);
//CHECK: extern int *fry(void);

void fret_driver(void) {
  int a = 0;
  int *b = &a;
  int *c = id(b);
  int *d = fry();
}
//CHECK: void fret_driver(void) {
//CHECK-NEXT: int a = 0;
//CHECK-NEXT: _Ptr<int> b = &a;
//CHECK-NEXT: _Ptr<int> c = id(b);
//CHECK-NEXT: int *d = fry();

typedef int *(*fooptr)(int*, int);

int *good_mut(int *a, int b) {
  *a = b;
  return a;
}
//CHECK: _Ptr<int> good_mut(_Ptr<int> a, int b) {

void fooptr_driver(void) {
  fooptr f = &good_mut;
  int a = 0;
  int *b = &a;
  int *c = f(b, 1);
}
//CHECK: void fooptr_driver(void) {
//CHECK-NEXT: _Ptr<_Ptr<int> (_Ptr<int> , int )>   f = &good_mut;
//CHECK-NEXT: int a = 0;
//CHECK-NEXT: _Ptr<int> b = &a;
//CHECK-NEXT: _Ptr<int> c = f(b, 1);

#define launder(x) (char*) x

void launder_driver(void) {
  int a = 0;
  int *b = &a;
  int *e = &a;
  char *d = launder(e);

  *b = 0;
  *d = 0;
}
//CHECK: void launder_driver(void) {
//CHECK-NEXT: int a = 0;
//CHECK-NEXT: _Ptr<int> b = &a;
//CHECK-NEXT: int *e = &a;
//CHECK-NEXT: char *d = launder(e);

typedef struct _D {
  char *a;
} D;
//CHECK: typedef struct _D {
//CHECK-NEXT: char *a;

typedef struct _E {
  char *b;
  int a;
} E;

#define launder1(x) ((E*) x->a)

void launder_driver2(void) {
  D d;
  D *pd = &d;
  E *pe = launder1(pd); 
}
//CHECK: void launder_driver2(void) {
//CHECK-NEXT: D d;
//CHECK-NEXT: _Ptr<D>  pd = &d;
//CHECK-NEXT: E *pe = launder1(pd);

void easy_cast_driver(void) {
  int a = 0;
  int *b = &a;
  char *c = (char *)b;

  *b = 0;
  *c = 1;
}
//CHECK: void easy_cast_driver(void) {
//CHECK-NEXT: int a = 0;
//CHECK-NEXT: int *b = &a;
//CHECK-NEXT: char *c = (char *)b;
