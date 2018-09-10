//
// These is a regression test case for 
// https://github.com/Microsoft/checkedc-clang/issues/543
//
// This test checks that we do not get duplicate error message for uses
// of arithmetic on void pointers in bounds expressions.

// RUN: %clang -cc1 -verify -fcheckedc-extension %s

_Array_ptr<void> f1(void) : bounds(_Return_value, _Return_value + 5);  // expected-error {{arithmetic on a pointer to void}}

void f2(_Array_ptr<void> p : bounds(p, p + 5)); // expected-error {{arithmetic on a pointer to void}}

void f3(_Array_ptr<void> p, _Array_ptr<void> q, _Array_ptr<int> buf : count(p - q)); // expected-error {{arithmetic on a pointers to void}}
