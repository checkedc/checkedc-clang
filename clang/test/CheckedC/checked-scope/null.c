// Tests that implementation-specific definitions of NULL can be used
// in checked scopes.

// RUN: %clang_cc1 -verify -fcheckedc-extension -Wno-unused-value %s
// expected-no-diagnostics

_Checked _Ptr<int> f(void) {
  _Ptr<int> p = ((void *)0);
  *((void *) 0);  // TODO: this should not be allowed in a checked scope.
  return p;
}
