// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test valid where clause cases.

// expected-no-diagnostics

void valid_cases_nullstmt(_Nt_array_ptr<char> p, _Nt_array_ptr<char> q,
                          int a, int b) {
  _Where a < 0;
  _Where a > 0;
  _Where a <= 0;
  _Where a >= 0;
  _Where a == 0;
  _Where a != 0;
  _Where 1 < 2;
  _Where 1 == 2;
  _Where a + b < a - b;
  _Where a + b < 1 + 2;
  _Where 1 + 2 > a + b;
  _Where a + 1 == b + 2;
  _Where a < 0 _And b > 0;
  _Where p : bounds(p, p + 1);
  _Where p : bounds(q, q + a);
  _Where p : count(0);
  _Where p : bounds(p, p + 1) _And q : bounds(q, q + 1);
  _Where a < 0 _And p : count(0) _And b > 0 _And q : bounds(q, q + 1);
  _Where a > 0 _And a > 1 _And a > 2 _And b < 0 _And b < 1 _And b < 2;
  _Where p : count(a) _And p : count(a + 1) _And p : count(a + 2);
  _Where q : count(0) _And q : bounds(q, q + 1) _And q : count(a);
  _Where (((((a == 0)))));
  _Where (a == 0) _And ((a == 0)) _And (((a == 0)));
}

int f();
void valid_cases_decl(_Nt_array_ptr<char> p, _Nt_array_ptr<char> q) {
  int a _Where a == 0 _And a != 0 _And p : bounds(p, p + a);
  int b = 0 _Where b != 0 _And p : count(b);
  int c = f() _Where p : bounds(p, p + c) _And c < 0;
  int d _Where p : bounds(p, p + d) _And q : count(d) _And d == 0;
  int e, f _Where e == 0 _And f == 0;
  int g _Where g == 0, h, i, j _Where j < 0;
  int k _Where k != 0 _And p : count(k), m, n, o _Where q : bounds(q, q + 1), r, s;
}

// Test where clauses on declarations outside a function.
_Nt_array_ptr<char> p;
_Nt_array_ptr<char> q;
int a _Where a == 0 _And a != 0 _And p : bounds(p, p + a);
int b = 0 _Where b != 0 _And p : count(b);
int c _Where p : bounds(p, p + c) _And q : count(c) _And c == 0;
int d, e _Where e == 0 _And d != 0;
int g _Where g == 0, h, i, j _Where j < 0;
int k _Where k != 0 _And p : count(k), m, n, o _Where q : bounds(q, q + 1), r, s;
