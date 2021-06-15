// RUN: %clang_cc1 -fsyntax-only -fno-checkedc-extension -verify %s

// PR9137
void f0(int x) : {}; // expected-error{{expected function body after function declarator}}
void f1(int x) try {}; // expected-error{{expected function body after function declarator}}
