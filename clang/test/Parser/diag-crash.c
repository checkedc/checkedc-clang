// RUN: %clang_cc1 -fsyntax-only -fno-checkedc-extension -verify %s

// Avoid preprocessor diag crash caused by a parser diag left in flight.

int foo: // expected-error {{expected ';' after top level declarator}}
#endif   // expected-error {{#endif without #if}}
