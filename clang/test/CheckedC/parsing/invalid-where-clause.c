// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test parsing errors for incorrect where clause.

void invalid_cases(_Nt_array_ptr<char> p, int a, int b) {
  _Where ; // expected-error {{invalid equality expression in where clause}} expected-error {{expected expression}}
  _Where ;; // expected-error {{invalid equality expression in where clause}} expected-error {{expected expression}}
  _Where _Where; // expected-error {{invalid equality expression in where clause}} expected-error {{expected expression}}
  _Where _And; // expected-error {{expected expression}} expected-error {{invalid equality expression in where clause}} expected-error {{expected expression}}
  _Where _And _And _And; // expected-error {{expected expression}} expected-error {{invalid equality expression in where clause}} expected-error {{expected expression}}
  _Where a; // expected-error {{expected equality or relational operator in where clause equality expression}}
  _Where a _Where a _Where a; // expected-error {{expected equality or relational operator in where clause equality expression}}
  _Where a _And; // expected-error {{expected equality or relational operator in where clause equality expression}} expected-error {{expected expression}}
  _Where x; // expected-error {{use of undeclared identifier 'x'}} expected-error {{invalid equality expression in where clause}}
  _Where p : ; // expected-error {{expected bounds expression}} expected-error {{invalid bounds expression in where clause}}
  _Where p : count(x); // expected-error {{use of undeclared identifier 'x'}} expected-error {{invalid bounds expression in where clause}}
  _Where q : count(0); // expected-error {{use of undeclared identifier q}}
  _Where (); // expected-error {{invalid equality expression in where clause}} expected-error {{expected expression}}
  _Where a = 0; // expected-error {{expected equality or relational operator in where clause equality expression}}
  _Where a == 1 _And a; // expected-error {{expected equality or relational operator in where clause equality expression}}
  _Where a _And a == 1; // expected-error {{expected equality or relational operator in where clause equality expression}} expected-error {{expected expression}}
  _Where a < 0 && x; // expected-error {{use of undeclared identifier 'x'}} expected-error {{invalid equality expression in where clause}}
  _Where 1; // expected-error {{expected equality or relational operator in where clause equality expression}}
  _Where x _And p : count(0); // expected-error {{use of undeclared identifier 'x'}} expected-error {{invalid equality expression in where clause}} expected-error {{expected expression}}
  _Where p : count(0) _And x; // expected-error {{use of undeclared identifier 'x'}} expected-error {{invalid equality expression in where clause}}
  _Where a == 1 _And p : Count(0); // expected-error {{expected bounds expression}} expected-error {{invalid bounds expression in where clause}}
  _Where p : Bounds(p, p + 1) _And a == 1; // expected-error {{expected bounds expression}} expected-error {{invalid bounds expression in where clause}} expected-error {{expected expression}}
  where a == 1; // expected-error {{use of undeclared identifier 'where'}}
  _Where a == 1 and a == 2; // expected-error {{expected ';'}} expected-error {{use of undeclared identifier 'and'}}
  _Where a == 1 // expected-error {{expected ';'}}
}
