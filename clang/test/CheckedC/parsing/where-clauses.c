// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test parsing of where clauses which declare program facts.

int func(const char *s : itype(_Nt_array_ptr<const char>));

void f1(_Nt_array_ptr<char> p : count(0)) {
  int a = func(p) _Where p : bounds(p, p + a);
  int b = func(p) _Where p : count(b);
  int c = func(p) _Where p;
  int d = func(p) _Where; // expected-error {{expected expression}}
  int e = func(p) _Where p :; // expected-error {{expected bounds expression}}
  int f = func(p) _Where p bounds(); // expected-error {{expected ';' at end of declaration}}
  int g = func(p) _Where p : bounds(); // expected-error {{expected expression}}
}
