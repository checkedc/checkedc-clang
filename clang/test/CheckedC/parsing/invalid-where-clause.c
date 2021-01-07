// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test parsing errors for incorrect where clause.

void invalid_cases(_Nt_array_ptr<char> p, int a, int b) {
  _Where ; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where ;; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where _Where ; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where a _Where a; // expected-error {{expected ';'}} expected-error {{incorrect where clause}}
  _Where _And ; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where a _And ; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where _And a ; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where x; // expected-error {{use of undeclared identifier 'x'}} expected-error {{incorrect where clause}}
  _Where p : ; // expected-error {{expected bounds expression}} expected-error {{incorrect where clause}}
  _Where q : count(0); // expected-error {{use of undeclared identifier q}} expected-error {{incorrect where clause}}
  _Where p : count(0) && q : count(0); // expected-error {{expected ';'}} expected-error {{incorrect where clause}}
  _Where (); //expected-error {{expected expression}} // expected-error {{incorrect where clause}}
}
