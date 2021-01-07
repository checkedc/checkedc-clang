// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test valid where clause cases.

// expected-no-diagnostics

void valid_cases(_Nt_array_ptr<char> p, _Nt_array_ptr<char> q,
                 int a, int b) {
  _Where a < 0;
  _Where a > 0;
  _Where a <= 0;
  _Where a >= 0;
  _Where a == 0;
  _Where a != 0;
  _Where p : bounds(p, p + 1);
  _Where p : count(0);
  _Where a < 0 _And b > 0;
  _Where p : bounds(p, p + 1) _And q : bounds(q, q + 1);
  _Where a < 0 _And p : count(0) _And b > 0 _And q : bounds(q, q + 1);
  _Where (a <= 0 && b >= 0) _And p : count(0);
  _Where a;
  _Where ((a));
  _Where 1;
  _Where a + b < 1;
  _Where a + 1 == b - 1;
  _Where 1 == 2;
  _Where a = 0;
  _Where a > 0 && a > 1 && a > 2 && b < 0 && b < 1 && b < 2;
  _Where a > 0 _And a > 1 _And a > 2 _And b < 0 _And b < 1 _And b < 2;
  _Where a > 0 && a > 1 _And a > 2 && b < 0 _And b < 1 && b < 2;
  _Where (a > 0 && a > 1) _And (a > 2 && b < 0) _And (b < 1) && (b < 2);
  _Where p : count(a) _And p : count(a + 1) _And p : count(a + 2);
  _Where q : count(0) _And q : bounds(q, q + 1) _And q : count(a);
}
