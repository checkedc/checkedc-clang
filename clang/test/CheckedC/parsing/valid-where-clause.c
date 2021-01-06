// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test valid where clause cases.

// expected-no-diagnostics

void valid_cases(_Nt_array_ptr<char> p, int a, int b) {
  _Where a < 0;
  _Where a > 0;
  _Where a <= 0;
  _Where a >= 0;
  _Where a == 0;
  _Where a != 0;
}
