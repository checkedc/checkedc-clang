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

//
// Constant-sized ranges
//

// Initializers
void f20(_Array_ptr<int> p: count(5)) {
  _Array_ptr<int> q1 : bounds(p, p + 5) = p; // No error expected
  _Array_ptr<int> q2 : bounds(p + 0, p + 5) = p; // No error expected
  _Array_ptr<int> q3 : bounds(p, p + 4) = p; // No error expected
  _Array_ptr<int> q4 : bounds(p + 1, p + 4) = p; // No error expected
  _Array_ptr<int> r : bounds(p, p + 6) = p; // expected-error {{declared bounds for 'r' are invalid after initialization}} \
                                            // expected-note {{destination bounds are wider than the source bounds}} \
                                            // expected-note {{destination upper bound is above source upper bound}} \
                                            // expected-note {{(expanded) declared bounds are 'bounds(p, p + 6)'}} \
                                            // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
  _Array_ptr<int> s : bounds(p - 1, p + 5) = p;  // expected-error {{declared bounds for 's' are invalid after initialization}} \
                                            // expected-note {{destination bounds are wider than the source bounds}} \
                                            // expected-note {{destination lower bound is below source lower bound}} \
                                            // expected-note {{(expanded) declared bounds are 'bounds(p - 1, p + 5)'}} \
                                            // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
  _Array_ptr<int> t : bounds(p - 1, p + 6) = p;  // expected-error {{declared bounds for 't' are invalid after initialization}} \
                                            // expected-note {{destination bounds are wider than the source bounds}} \
                                            // expected-note {{destination lower bound is below source lower bound and destination upper bound is above source upper bound}} \
                                            // expected-note {{(expanded) declared bounds are 'bounds(p - 1, p + 6)'}} \
                                            // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
}

// Assignments
void f21(_Array_ptr<int> p: count(5)) {
  _Array_ptr<int> q1 : bounds(p, p + 5) = 0;
   q1 = p; // No error expected
  _Array_ptr<int> q2 : bounds(p + 0, p + 5) = 0;
  q2 = p; // No error expected
  _Array_ptr<int> q3 : bounds(p, p + 4) = 0;
  q3 = p; // No error expected
  _Array_ptr<int> q4 : bounds(p + 1, p + 4) = 0;
  q4 = p; // No error expected
  _Array_ptr<int> r : bounds(p, p + 6) = 0;
  r = p; // expected-error {{declared bounds for r are invalid after assignment}} \
         // expected-note {{destination bounds are wider than the source bounds}} \
         // expected-note {{destination upper bound is above source upper bound}} \
         // expected-note {{(expanded) declared bounds are 'bounds(p, p + 6)'}} \
         // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
  _Array_ptr<int> s : bounds(p - 1, p + 5) = 0;
  s = p;  // expected-error {{declared bounds for s are invalid after assignment}} \
          // expected-note {{destination bounds are wider than the source bounds}} \
          // expected-note {{destination lower bound is below source lower bound}} \
          // expected-note {{(expanded) declared bounds are 'bounds(p - 1, p + 5)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
  _Array_ptr<int> t : bounds(p - 1, p + 6) = 0;
  t = p;  // expected-error {{declared bounds for t are invalid after assignment}} \
          // expected-note {{destination bounds are wider than the source bounds}} \
          // expected-note {{destination lower bound is below source lower bound and destination upper bound is above source upper bound}} \
          // expected-note {{(expanded) declared bounds are 'bounds(p - 1, p + 6)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
}

void constant_sized_bound1(_Array_ptr<int> p : count(5)) {
}

// Argument points into the middle of a range.
void constant_sized_bound2(_Array_ptr<int> p : bounds(p - 1, p + 4)) {
}

// TODO: variants of constant_sized bound where the argument points into the middle
// of a range.  We need reassociation of arithmetic expressions so that constant-folding
// works properly.  Example:
//  void constant_sized_bound2(_Array_ptr<int> p : bounds(p - 1, p + 5)
// If we have an argument variable v : bounds(v, v + 3), we'll ahve problems
// when v is substitu
void f22(_Array_ptr<int> q : count(5)) {
  constant_sized_bound1(q);  // No error_expected
  _Array_ptr<int> r : count(4) = 0;
  constant_sized_bound1(r);   // expected-error {{argument does not meet declared bounds for 1st parameter}} \
          // expected-note {{destination bounds are wider than the source bounds}} \
          // expected-note {{destination upper bound is above source upper bound}} \
          // expected-note {{(expanded) expected argument bounds are 'bounds(r, r + 5)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(r, r + 4)'}}
  _Array_ptr<int> s : count(6) = 0;
  constant_sized_bound1(s);  // No error expected
  _Array_ptr<int> t1 : bounds(t1 + 1, t1 + 6) = 0;
  constant_sized_bound1(t1);  // expected-error {{argument does not meet declared bounds for 1st parameter}} \
          // expected-note {{destination lower bound is below source lower bound}} \
          // expected-note {{(expanded) expected argument bounds are 'bounds(t1, t1 + 5)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(t1 + 1, t1 + 6)'}}
  _Array_ptr<int> t2 : bounds(t2 - 1, t2 + 4) = 0;
  constant_sized_bound1(t2);  // expected-error {{argument does not meet declared bounds for 1st parameter}} \
          // expected-note {{destination upper bound is above source upper bound}} \
          // expected-note {{(expanded) expected argument bounds are 'bounds(t2, t2 + 5)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(t2 - 1, t2 + 4)'}}
  constant_sized_bound2(q); // expected-error {{argument does not meet declared bounds for 1st parameter}} \
          // expected-note {{destination lower bound is below source lower bound}} \
          // expected-note {{(expanded) expected argument bounds are 'bounds(q - 1, q + 4)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(q, q + 5)'}}
  constant_sized_bound2(q + 1);  // expected-warning {{argument may not meet declared bounds for 1st parameter}} \
                                 // expected-note {{(expanded) expected argument bounds are 'bounds(q + 1 - 1, q + 1 + 4)'}} \
                                 // expected-note {{(expanded) inferred bounds are 'bounds(q, q + 5)'}}
                                // TODO: no warning/error expected.  We need reassocation of arithmetic
                                // expressions to avoid the warning here

}

struct S {
  int x;
  int y;
};

void test_ptr_void_cast(_Ptr<void> p) {
  _Ptr<int> ip = (_Ptr<int>) p; // expected-error {{cast source bounds are too narrow for '_Ptr<int>'}} \
                                // expected-note {{(expanded) required bounds are 'bounds((_Ptr<int>)p, (_Ptr<int>)p + 1)'}} \
                                // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 1)'}}
  _Ptr<struct S> sp = (_Ptr<struct S>) p; // expected-error {{cast source bounds are too narrow for '_Ptr<struct S>'}} \
                                          // expected-note {{(expanded) required bounds are 'bounds((_Ptr<struct S>)p, (_Ptr<struct S>)p + 1)'}} \
                                          // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 1)'}}
  // TODO: _Ptr<char> cp = (_Ptr<char>) p;  _Ptr<void> should be treated as being 1 character wide, not 0 characters wide.
}
