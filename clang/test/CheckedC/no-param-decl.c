// Bug repro from testing bounds-safe interface implementation.
//
// Initializing parameter assignments do not have declarations available
// for parameters for  indirect function calls.  This caused a null pointer
// dereference in the bound-safe interface implementation.
//
// RUN: %clang_cc1 -verify -fcheckedc-extension %s
// expected-no-diagnostics

void bad_mut(int *a) {
}

void bad_mut_driver(void) {
  _Ptr<void (int*)> bad_mut_ptr  = &bad_mut;
  int a = 0;
  int *b = &a;
  bad_mut_ptr(b);
}
