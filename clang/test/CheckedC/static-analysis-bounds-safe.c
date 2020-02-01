// Test case for SimpleBounds checker when there is bounds-safe
// information available for the bounds checking.
//
// REQUIRES: z3
//
// RUN: %clang_cc1 -analyze -analyzer-checker alpha.security.SimpleBounds %s -verify
// RUN: %clang_cc1 -analyze -analyzer-checker alpha.security.ArrayBoundV2 -DCLANG_CHECKERS %s -verify
// 

int foo(int *p : count(n), int n);
int foo(int *p, int n)
{
  int *a = p;

  int k = n + n;
  int t = (k & 1) | ((k & 1) ^ 1); // t will always evaluate to '1'

  a[t - 1] = 1;
  a[n / 2] = 1;

#ifdef CLANG_CHECKERS
  *(a + k) = 1; // expected-no-diagnostics
#else
  *(a + k) = 1; // expected-warning {{Access out-of-bound array element (buffer overflow)}}
#endif

  return 0;
}

int bar(int *p : count(n + m), int n, int m);
int bar(int *p : count(n + m), int n, int m)
{
#ifdef CLANG_CHECKERS
  return p[n * m]; // expected-no-diagnostics
#else
  return p[n * m]; // expected-warning {{Access out-of-bound array element (buffer overflow}}
#endif
}


