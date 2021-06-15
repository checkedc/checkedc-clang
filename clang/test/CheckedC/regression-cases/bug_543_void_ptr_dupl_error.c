//
// These is a regression test case for 
// https://github.com/Microsoft/checkedc-clang/issues/543
//
// This test checks that we do not get duplicate error message for uses
// of arithmetic on checked void pointers and checked function
// pointers in bounds expressions.

// RUN: %clang -cc1 -verify -fcheckedc-extension %s

// Test checked pointers to void.
_Array_ptr<void> f1(void) : bounds(_Return_value, _Return_value + 5);  // expected-error {{arithmetic on a pointer to void}}

void f2(_Array_ptr<void> p : bounds(p, p + 5)); // expected-error {{arithmetic on a pointer to void}}

void f3(_Array_ptr<void> p, _Array_ptr<void> q, _Array_ptr<int> buf : count(p - q)); // expected-error {{arithmetic on pointers to void}}

// Test checked function pointers.  Other error messages should prevent
// pointer arithmetic on them in bounds.

// Test _Array_ptrs.  Declaring an _Array_ptr of function type is not allowed.
void f4(_Array_ptr<int (int)> pf, _Array_ptr<void> code : bounds(pf, pf + 1));  // expected-error {{declared as _Array_ptr to function}}

void f5(_Array_ptr<int (int)> pf, _Array_ptr<int (int)> pg, _Array_ptr<void> code : count(pf - pg));  // expected-error 2 {{declared as _Array_ptr to function}}

// Test _Ptr.  Declaring a _Ptr to function types is not allowed.
void f6(_Ptr<int (int)> pf, _Ptr<void> code : bounds(pf, pf + 1)); // expected-error {{arithmetic on _Ptr type}}

void f7(_Ptr<int (int)> pf, _Ptr<int (int)> pg, _Array_ptr<void> code : bounds(pf, pf + (pg - pf))); // expected-error {{arithmetic on _Ptr type}}
