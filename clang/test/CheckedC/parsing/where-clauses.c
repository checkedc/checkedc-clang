// RUN: %clang_cc1 -fsyntax-only -verify %s

// Test parsing of where clauses that declare program facts.

extern unsigned long strlen(const char *s : itype(_Nt_array_ptr<const char>));
extern unsigned long foo(const char *s : itype(_Nt_array_ptr<const char>));

void f1(_Nt_array_ptr<char> p : count(0)) {
  int a = strlen(p) _Where p : bounds(p, p + a);
  int b = strlen(p) _Where p : count(b);
  int c = strlen(p) _Where p;
  int d = strlen(p) _Where; // expected-error {{expected expression}} expected-error {{incorrect where clause}}
  int e = strlen(p) _Where p :; // expected-error {{expected bounds expression}} expected-error {{incorrect where clause}}
  int f = strlen(p) _Where p bounds(); // expected-error {{expected ';' at end of declaration}}
  int g = strlen(p) _Where p : bounds(); // expected-error {{expected expression}} expected-error {{incorrect where clause}}

  int h = 0;
  h = strlen(p) _Where p : bounds(p, p + a);

  for (h = strlen(p) _Where p : bounds(p, p + h); ;) {}

  for (int i = strlen(p) _Where p : bounds(p, p + i); ;) {}
}

void f2(_Nt_array_ptr<char> p : count(0)) {
  int x = 0;
  int a = foo(p) _Where p : bounds(p, p + a); // expected-error {{incorrect where clause}}
  int b = foo(p) _Where x = 1; // expected-error {{incorrect where clause}}
}
