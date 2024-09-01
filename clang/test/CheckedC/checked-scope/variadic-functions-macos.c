// UNSUPPORTED: !darwin

// Test calls to variadic functions in checked scopes.
// Some -Wformat error messages are different between Linux, Windows
// and MacOS.   This file contains MacOS specific ests. The common
// tests are in variadic-functions.c.

// RUN: %clang_cc1 -fcheckedc-extension -verify \
// RUN: -verify-ignore-unexpected=note %s

int printf(const char *format : itype(_Nt_array_ptr<const char>), ...);
int MyPrintf(const char *format : itype(_Nt_array_ptr<const char>), ...)
  __attribute__((format(printf, 1, 2)));

int scanf(const char *format : itype(_Nt_array_ptr<const char>), ...);
int MyScanf(const char *format : itype(_Nt_array_ptr<const char>), ...)
  __attribute__((format(scanf, 1, 2)));

void f1 (_Nt_array_ptr<char> p) {
_Checked {
  printf("%Z", p); // expected-error {{invalid conversion specifier 'Z'}}
  MyPrintf("%Z", p); // expected-error {{invalid conversion specifier 'Z'}}
  scanf("%Z", p); // expected-error {{invalid conversion specifier 'Z'}}
  MyScanf("%Z", p); // expected-error {{invalid conversion specifier 'Z'}}

  printf("%Li", (long long) 42); // expected-error {{length modifier 'L' results in undefined behavior or no effect with 'i' conversion specifier}}
  MyPrintf("%Li", (long long) 42); // expected-error {{length modifier 'L' results in undefined behavior or no effect with 'i' conversion specifier}}
  scanf("%Li", (long long) 42); // expected-error {{length modifier 'L' results in undefined behavior or no effect with 'i' conversion specifier}} expected-error {{format specifies type 'long long *' but the argument has type 'long long'}}
  MyScanf("%Li", (long long) 42); // expected-error {{length modifier 'L' results in undefined behavior or no effect with 'i' conversion specifier}} expected-error {{format specifies type 'long long *' but the argument has type 'long long'}}
}
}
