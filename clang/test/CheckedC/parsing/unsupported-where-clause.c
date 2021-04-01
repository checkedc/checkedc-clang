// RUN: %clang_cc1 -fsyntax-only -verify %s

// Various cases that currently do not support where clauses. We are choosing
// not to support these in order to strike a balance between extensive support
// for where clauses and the resulting usefulness but not because we need to
// handle them separately.

void f(int i, int j, int k);
unsigned strlen_test(_Nt_array_ptr<char> p);

void unsupported_cases(int i, int j, int k) {
  // According to the C11 spec, the conditionals of selection-statements (like
  // if and switch) and iteration-statements (like while, do-while and for) are
  // expressions (and not expression-statements) and they evaluate to a scalar.
  // So we need separate handling of where clauses inside these conditionals.

  if (i _Where i > 0) {} // expected-error {{expected ')'}} expected-note {{to match this '('}}
  if (i = 0 _Where i > 0) {} // expected-error {{expected ')'}} expected-note {{to match this '('}} expected-warning {{using the result of an assignment as a condition without parentheses}} expected-note {{place parentheses around the assignment to silence this warning}} expected-note {{use '==' to turn this assignment into an equality comparison}}
  if (i == 0 _Where i > 0) {} // expected-error {{expected ')'}} expected-note {{to match this '('}}

  _Nt_array_ptr<char> p = ""; // expected-note {{(expanded) declared bounds are 'bounds(p, p + 0)'}}
  if ((i = strlen_test(p) _Where p : bounds(p, p + i)) > 0) {} // expected-error {{expected ')'}} expected-note {{to match this '('}}

  while (i _Where i > 0) {} // expected-error {{expected ')'}} expected-note {{to match this '('}}

  do {} while (i _Where i > 0); // expected-error {{expected ')'}} expected-note {{to match this '('}}

  // Where clauses on parameters in function calls are currently not supported.
  f(i _Where i > 0, j, k); // expected-error {{expected ')'}} expected-note {{to match this '('}}
  f(i, (j=1 _Where j > 0, j+2), k); // expected-error {{expected ')'}} expected-note {{to match this '('}}

  // Where clauses on struct members are currently not supported.
  struct S { int a _Where a != 0; }; // expected-error {{expected ';' at end of declaration list}}

  for (; p < p + 1 && *p; p++ _Where p : bounds(p, p + 1)) {} // expected-error {{expected ')'}} expected-note {{to match this '('}} expected-warning {{cannot prove declared bounds for 'p' are valid after increment}} expected-note {{(expanded) inferred bounds are 'bounds(p - 1, p - 1 + 0)'}}

  // The initializer of a for-loop is processed as part of processing the
  // for-loop itself (and not as part of processing an ExprStmt). So we need
  // separate handling of where clauses inside a for-loop.
  for (i = 0 _Where i > 0; i > 0; i++) {} // expected-error {{expected ';' in 'for' statement specifier}} expected-error {{expected expression}} expected-error {{expected ')'}} expected-error {{expected ';' after expression}} expected-error {{expected expression}} expected-warning {{relational comparison result unused}} expected-note {{to match this '('}}
}
