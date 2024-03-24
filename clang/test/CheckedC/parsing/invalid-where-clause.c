// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test parsing errors for incorrect where clause.

int f();

void invalid_cases_nullstmt(_Nt_array_ptr<char> p, int a, int b) {
  _Where ; // expected-error {{expected bounds declaration or equality expression in where clause}}
  _Where ;;;;; // expected-error {{expected bounds declaration or equality expression in where clause}}
  _Where _Where; // expected-error {{expected bounds declaration or equality expression in where clause}} expected-error {{expected bounds declaration or equality expression in where clause}}
  _Where _And; // expected-error {{expected bounds declaration or equality expression in where clause}} expected-error {{expected bounds declaration or equality expression in where clause}}
  _Where _And _And _And; // expected-error {{expected bounds declaration or equality expression in where clause}} expected-error {{expected bounds declaration or equality expression in where clause}} expected-error {{expected bounds declaration or equality expression in where clause}} expected-error {{expected bounds declaration or equality expression in where clause}}
  _Where a; // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}}
  _Where a _Where a _Where a; // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}} expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}} expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}}
  _Where a _And; // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}} expected-error {{expected bounds declaration or equality expression in where clause}}
  _Where x; // expected-error {{use of undeclared identifier 'x'}}
  _Where p : ; // expected-error {{expected bounds expression}}
  _Where p : count(x); // expected-error {{use of undeclared identifier 'x'}}
  _Where q : count(0); // expected-error {{use of undeclared identifier q}}
  _Where a = 0; // expected-error {{expected comparison operator in equality expression}}
  _Where a == 1 _And a; // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}}
  _Where a _And a == 1; // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}}
  _Where a < 0 _And x; // expected-error {{use of undeclared identifier 'x'}}
  _Where 1; // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}}
  _Where x _And p : count(0); // expected-error {{use of undeclared identifier 'x'}}
  _Where p : count(0) _And x; // expected-error {{use of undeclared identifier 'x'}}
  _Where a == 1 _And p : Count(0); // expected-error {{expected bounds expression}}
  _Where p : Bounds(p, p + 1) _And a == 1; // expected-error {{expected bounds expression}}
  where a == 1; // expected-error {{use of undeclared identifier 'where'}}
  _Where a == 1 and a == 2; // expected-error {{expected ';'}} expected-error {{use of undeclared identifier 'and'}}
  _Where a == 1 // expected-error {{expected ';'}}
  _Where f(); // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}}
  _Where f() == 1; // expected-error {{call expression not allowed in expression}}
  _Where 1 != f(); // expected-error {{call expression not allowed in expression}}
  _Where a++ < 1; // expected-error {{increment expression not allowed in expression}}
  _Where a _And p : _And q : count(0) _And x _And a = 1 _And f() < 0 _And; // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}} expected-error {{expected bounds expression}} expected-error {{use of undeclared identifier q}} expected-error {{use of undeclared identifier 'x'}} expected-error {{expected comparison operator in equality expression}} expected-error {{call expression not allowed in expression}} expected-error {{expected bounds declaration or equality expression in where clause}}
}

void invalid_cases_exprstmt(_Nt_array_ptr<char> p, int a) {
  a = 0 _Where a _And p : _And q : count(0) _And x _Where b = 1 _And f() < 0 _And; // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}} expected-error {{expected bounds expression}} expected-error {{use of undeclared identifier q}} expected-error {{use of undeclared identifier 'x'}} expected-error {{use of undeclared identifier 'b'}} expected-error {{call expression not allowed in expression}} expected-error {{expected bounds declaration or equality expression in where clause}}

  int x, y;
  // For an ExprStmt with commas, a where clause cannot be attached after each
  // comma. This is because the entire expression including commas is parsed as
  // one ExprStmt. Hence a where clause can only be attached at the end of the
  // entire ExprStmt.
  x = 1 _Where x > 0, y = 2 _Where y > 1; // expected-error {{expected ';' after expression}}
}

void f1(int a _Where a, _Nt_array_ptr<int> p : count(0) _Where p :); // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}} expected-error {{expected bounds expression}}

void f2(int *p : itype(_Ptr<int>) _Where p, int n); // expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}}

void f3(int a _Where a == 1, a == 1); // expected-error {{unknown type name 'a'}} expected-error {{expected ')'}} expected-note {{to match this '('}}

void f4(int a _Where a == 1, b == 1); // expected-error {{unknown type name 'b'}} expected-error {{expected ')'}} expected-note {{to match this '('}}

void f5(int a _Where a,a); // expected-error {{redefinition of parameter 'a'}} expected-error {{invalid expression in where clause, expected bounds declaration or equality expression}} expected-warning {{type specifier missing, defaults to 'int'}} expected-note {{previous declaration is here}}

void f6(_Where 1 == 0); // expected-error {{expected parameter declarator}} expected-error {{expected ')'}} expected-note {{to match this '('}}

void f7(_Nt_array_ptr<int> p : count(n) _Where p : count(n)), int n); // expected-error {{use of undeclared identifier 'n'}} expected-error {{use of undeclared identifier 'n'}} expected-error {{expected identifier or '('}} expected-error {{expected ';' after top level declarator}} expected-error {{extraneous ')' before ';'}}

void f8(int a _Where (1 == 0); // expected-error {{expected ')'}} expected-note {{to match this '('}}

void f9(int a _Where (1 == 0))); // expected-error {{expected function body after function declarator}}

void f10(int a _Where ((((1 == 0); // expected-error {{expected ')'}} expected-note {{to match this '('}} // expected-error {{expected function body after function declarator}}
