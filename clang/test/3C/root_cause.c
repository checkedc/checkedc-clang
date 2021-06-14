// TODO: Fix the failures in this test that were introduced while the diagnostic
// verifier was disabled
// (https://github.com/correctcomputation/checkedc-clang/issues/609).
// XFAIL: *

// RUN: 3c -base-dir=%S -alltypes -warn-root-cause %s -- -Xclang -verify -Wno-everything

// This test is unusual in that it checks for the errors in the code

#include <stdlib.h>

void *x; // expected-warning {{Default void* type}}

void test0() {
  int *a;
  char *b;
  a = b; // expected-warning {{Cast from char * to int *}}

  int *c;
  (char *)c; // expected-warning {{Cast from int * to char *}}

  int *e;
  char *f;
  f = (char *)e; // expected-warning {{Cast from int * to char *}}
}

void test1() {
  int a;
  int *b;
  b = malloc(sizeof(int)); // expected-warning {{Bad pointer type solution}}
  b[0] = 1;

  union u {
    int *a; // expected-warning {{Union or external struct field encountered}}
    int *b; // expected-warning {{Union or external struct field encountered}}
  };

  void (*c)(void);
  c++; // expected-warning {{Pointer arithmetic performed on a function pointer}}

  int *d = malloc(1); // expected-warning {{Unsafe call to allocator function}}
}

extern int *
    glob; // expected-warning {{External global variable glob has no definition}}

void (*f)(void *); // expected-warning {{Default void* type}}

typedef struct {
  int x;
  float f;
} A, *PA;
// expected-warning@-1 {{Unable to rewrite a typedef with multiple names}}
