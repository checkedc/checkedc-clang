// RUN: %clang_cc1 -fsyntax-only -verify \
// RUN: -verify-ignore-unexpected=note \
// RUN: -verify-ignore-unexpected=warning \
// RUN: -verify-ignore-unexpected=error %s

// Test valid where clause cases.

// expected-no-diagnostics

// Test where clauses on null statements.
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
// Test where clauses on variable declarations inside a function.
void valid_cases_decl(_Nt_array_ptr<char> p, _Nt_array_ptr<char> q) {
  int a _Where a == 0 _And a != 0 _And p : bounds(p, p + a);
  int b = 0 _Where b != 0 _And p : count(b);
  int c = f() _Where p : bounds(p, p + c) _And c < 0;
  int d _Where p : bounds(p, p + d) _And q : count(d) _And d == 0;
  int e, f _Where e == 0 _And f == 0;
  int g _Where g == 0, h, i, j _Where j < 0;
  int k _Where k != 0 _And p : count(k), m, n, o _Where q : bounds(q, q + 1), r, s;
  int arr1[3] = {1, 2, 3} _Where 0 < 1, arr2[2] = {1, 2} _Where 1 > 0;
  _Nt_array_ptr<char> p1 : count(0) = "" _Where p1 : bounds(p1, p1 + 1);
}

// Test where clauses on variable declarations outside a function.
_Nt_array_ptr<char> p;
_Nt_array_ptr<char> q;
int a _Where a == 0 _And a != 0 _And p : bounds(p, p + a);
int b = 0 _Where b != 0 _And p : count(b);
int c _Where p : bounds(p, p + c) _And q : count(c) _And c == 0;
int d, e _Where e == 0 _And d != 0;
int g _Where g == 0, h, i, j _Where j < 0;
int k _Where k != 0 _And p : count(k), m, n, o _Where q : bounds(q, q + 1), r, s;
int arr1[3] = {1, 2, 3} _Where 0 < 1, arr2[2] = {1, 2} _Where 1 > 0;
_Nt_array_ptr<char> p1 : count(0) = "" _Where p1 : bounds(p1, p1 + 1);

// Test where clauses on function parameters.
void f1(int a _Where a < 0, int b, int c _Where c < 0, int *d : itype(_Ptr<int>) _Where d == 0) {}
void f2(_Nt_array_ptr<char> p _Where p : bounds(p, p + n) _And n > 0, int n);
void f3(_Nt_array_ptr<char> p : count(n) _Where n > 0 _And p : count(n), int n) {}
void f4(_Nt_array_ptr<char> p : count(n) _Where p : count(n) _And p : count(n), int n);
void f5(_Nt_array_ptr<char> p : count(n), int a _Where p : count(n) _And n > 0 _And a < 0, int n) {}
void f6(int *p : itype(_Ptr<int>) _Where p == 0 _And n > 0, int n);
void f7(int a _Where (((((a == 0))))));

// Test where clauses on ExprStmts.
void valid_cases_exprstmt(_Nt_array_ptr<char> p, int a) {
  int b;
  a = 0 _Where a < 1 _And a >= 0;
  b = a _Where a > b _And a != b;
  p = 0 _Where p : bounds(p, p + 1) _And a < 1;
L1: a = 0 _Where a > 1;

  // For an ExprStmt with commas, a where clause can be attached at the end of
  // the ExprStmt. This is because the entire expression including commas is
  // parsed as one ExprStmt. Hence a where clause can only be attached at the
  // end of this ExprStmt.
  int x, y, z;
  x = 1, y = 2, z = 3 _Where x > 0 _And y > 1 _And z > 2;

  for (int i = 0 _Where i != 0; i > 0; i ++) {}
}

_Checked
void iface(unsigned long n _Where n > 0) {
    return;
}

_Checked
void iface_test(void) {
  iface(50);
}
