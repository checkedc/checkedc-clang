// Tests for checking that bounds declarations hold after assignments to
// variables and initialization of variables.  Because the static checker is
// mostly unimplemented, we only issue warnings when bounds declarations
// cannot be provided to hold.
//
// RUN: %clang -cc1 -fcheckedc-extension -Wcheck-bounds-decls -verify %s

// Initialization by null - no warning.
void f1(_Array_ptr<int> p : bounds(p, p + x), int x) {
    _Array_ptr<int> r : bounds(p, p + x) = 0;
}

// Initialization by expression with syntactically identical
// normalized bounds - no warning.
void f2(_Array_ptr<int> p : bounds(p, p + x), int x) {
  _Array_ptr<int> r : bounds(p, p + x) = p;
}

// Initialization by expression without syntactically identical
// normalized bounds - warning expected.
void f3(_Array_ptr<int> p : bounds(p, p + x), int x) {
  _Array_ptr<int> r : count(x) = p;     // expected-warning {{may be invalid}} \
                                        // expected-note {{(expanded) declared bounds are 'bounds(r, r + x)'}} \
                                        // expected-note {{(expanded) inferred bounds are 'bounds(p, p + x)'}}
}


// Assignment of null - no warning.
void f4(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : bounds(p, p + x) = 0;
  r = 0;
}

// Assignment of expression with syntactically identical normalized bounds -
//- no warning.
void f5(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : bounds(p, p + x) = p;
  r = p;
}

// Assignment of expression without syntactically identical normalized bounds -
// no warning.
void f6(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : count(x) = 0;
  r = p;      // expected-warning {{may be invalid}} \
              // expected-note {{(expanded) declared bounds are 'bounds(r, r + x)'}} \
              // expected-note {{(expanded) inferred bounds are 'bounds(p, p + x)'}}
}

// Parameter passing

void test_f1(_Array_ptr<int> arg : bounds(arg, arg + 1));

void test_f2(_Array_ptr<int> arg : count(1)) {
}

void test_calls(_Array_ptr<int> arg1 : count(1)) {
  // Passing null, no warning
  test_f1(0);

  _Array_ptr<int> t1 : bounds(t1, t1 + 1) = 0;
  // Syntactically identical bounds
  test_f1(t1);

  // Syntactically identical after expanding count
  test_f2(t1);
}
