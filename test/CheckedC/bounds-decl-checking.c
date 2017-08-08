// Tests for checking that bounds declarations hold after assignments to
// variables and initialization of variables.  Because the static checker is
// mostly unimplemented, we only issue warnings when bounds declarations
// cannot be provided to hold.
//
// RUN: %clang -cc1 -fcheckedc-extension -Wcheck-bounds-decls -verify -verify-ignore-unexpected=note %s

// Initialization by null - no warning.
void f1(_Array_ptr<int> p : bounds(p, p + x), int x) {
    _Array_ptr<int> r : bounds(p, p + x) = 0;
}

// Initialization by expression with syntactically identical 
// normalized bounds - no warning.
void f2(_Array_ptr<int> p : bounds(p, p + x), int x) {
  _Array_ptr<int> r : bounds(p, p + x) = 0;
}

// Initialization by expression without syntactically identical 
// normalized bounds - warning expected.
void f3(_Array_ptr<int> p : bounds(p, p + x), int x) {
  _Array_ptr<int> r : count(x) = p;     // expected-warning {{may be invalid}}
}


// Assignment of null - no warning.
void f4(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : bounds(p, p + x) = 0;
  r = 0;
}

// Assignment of expression with syntactically identical normalized bounds -
//- no warning.
void f5(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : bounds(p, p + x) = 0;
  r = p;
}

// Assignment of expression without syntactically identical normalized bounds -
// no warning.
void f6(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : count(x) = 0;
  r = p;      // expected-warning {{may be invalid}}
}
