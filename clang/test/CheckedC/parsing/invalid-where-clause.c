// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test parsing errors for incorrect where clause.

void invalid_cases(_Nt_array_ptr<char> p, int a, int b) {
  _Where ; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where ;; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where _Where ; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where a _Where a; // expected-error {{incorrect where clause}}
  _Where a && ; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  _Where && a ; // expected-error {{use of undeclared label 'a'}} expected-error {{incorrect where clause}}
  _Where x; // expected-error {{use of undeclared identifier 'x'}} expected-error {{incorrect where clause}}
  _Where p : ; // expected-error {{expected bounds expression}} expected-error {{incorrect where clause}}
  _Where q : count(0); // expected-error {{use of undeclared identifier q}} expected-error {{incorrect where clause}}
  _Where 0; // expected-error {{incorrect where clause}}
  _Where 1 < 2; // expected-error {{incorrect where clause}}
  _Where 1 + 2 < 3 + 4; // expected-error {{incorrect where clause}}
  _Where a + 1 < a + 1; // expected-error {{incorrect where clause}}
  _Where a + b < a + b; // expected-error {{incorrect where clause}}
  _Where a = 0; // expected-error {{incorrect where clause}}
  _Where (); //expected-error {{expected expression}} // expected-error {{incorrect where clause}}
}
