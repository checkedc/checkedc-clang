// Tests for clang-specific error messages during parsing of Checked C
// extensions.
//
// The Checked C repo contains many tests of parsing as part
// of its extension conformance test suite that also check clang error
// messages.  The extension conformance tests are designed to test overall
// compiler compliance with the Checked C specification.  This file is
// for more detailed tests of error messages, such as notes and correction 
// hints emitted as part of clang diagnostics.
//
// RUN: %clang_cc1 -verify -fcheckedc-extension %s

// Misplace a return bounds expression for a function with a complex
// declarator.   Here we expect the compiler to emit a note for where
// to place the return bounds expression.

// f32 is a function that returns a pointer to an array of 10 integers.  The
// return bounds expression must be part of the function declarator and
// should not follow the array declarator.
int(*(f32(int arg[10][10])))[10] : count(10); // expected-error {{unexpected bounds expression after declarator}} expected-note {{if this is a return bounds declaration for 'f32', place it after the ')'}}

int(*(f32(int arg[10][10])))[10] : count(10) { // expected-error {{unexpected bounds expression after declarator}} expected-note {{if this is a return bounds declaration for 'f32', place it after the ')'}}
  return arg;
}

// A return bounds expression cannot follow a parenthesized function
// declarator.

int *(f33(int i)) : count(10); // expected-error {{unexpected bounds expression after declarator}} expected-note {{if this is a return bounds declaration for 'f33', place it after the ')'}}

int *(f33(int i)) : count(10) { // expected-error {{unexpected bounds expression after declarator}} expected-note {{if this is a return bounds declaration for 'f33', place it after the ')'}}
  return 0;
}

// For bounds-safe interface types,  we make a best effort to report multiple
// errors during checking of bounds-safe interface types.  Test this
// functionality.

int f1(int *a : itype(char[])) { // expected-error {{type must be a checked type}} expected-error {{mismatch between interface type 'char *' and declared type 'int *'}}
}

int f2(int a : itype(int[])) { // expected-error {{interface type only allowed for a declaration with pointer or array type}} expected-error {{type must be a checked type}}
}

int f3(int a : itype(char)) { // expected-error {{interface type only allowed for a declaration with pointer or array type}} expected-error {{type must be a pointer or array type}}
}
