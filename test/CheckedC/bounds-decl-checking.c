// Tests for checking that bounds declarations hold after assignments to
// variables and initialization of variables.  Because the static checker is
// mostly unimplemented, we only issue warnings when bounds declarations
// cannot be provided to hold.
//
// RUN: %clang -cc1 -fcheckedc-extension -Wcheck-bounds-decls -verify -verify-ignore-unexpected=note %s

void f1(_Array_ptr<int> p : bounds(p, p + x), int x) {
    _Array_ptr<int> r : bounds(p, p + x) = 0;
    r = p;
}

void f2(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : count(x) = 0;
  r = p;  // expected-warning {{may not be true}}
}
