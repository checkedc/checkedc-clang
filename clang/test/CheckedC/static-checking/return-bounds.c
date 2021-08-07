// Tests for checking that the inferred bounds of a return value imply the
// declared return bounds for a function.  Because the static checker is
// mostly unimplemented, we only issue warnings when return bounds cannot
// be proved to hold.
//
// RUN: %clang_cc1 -fcheckedc-extension -Wcheck-bounds-decls -verify %s

//
// Test null bounds, bounds(unknown), and bounds(any)
//

_Array_ptr<int> f1(void) : count(1) {
  return 0;
}

_Array_ptr<char> f2(_Array_ptr<char> p : bounds(unknown)) {
  return p;
}

_Array_ptr<int> f3(_Array_ptr<int> p : bounds(unknown)) : bounds(unknown) {
  return p;
}

_Array_ptr<char> f4(_Array_ptr<char> p : bounds(unknown)) : count(1) { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + 1)'}}
  return p; // expected-error {{return value has unknown bounds, bounds expected because the function 'f4' has bounds}}
}

//
// Test no warnings or errors
//

_Array_ptr<char> f5(_Array_ptr<char> p : count(2)) : bounds(p, p + 2) {
  _Array_ptr<char> q : count(2) = p;
  return q;
}

_Nt_array_ptr<char> f6(void) : count(3) {
  return "abcd";
}

//
// Test bounds warnings
//

_Nt_array_ptr<char> f7(_Nt_array_ptr<char> p, int test) : count(0) { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + 0)'}}
  if (test)
    return p;
  return p + 1; // expected-warning {{cannot prove return value bounds imply declared return bounds for 'f7'}} \
                // expected-note {{(expanded) inferred return value bounds are 'bounds(p, p + 0)'}}
}

_Array_ptr<int> f8(int i, int j) : count(i) { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + i)'}}
  i = j + 1;
  _Array_ptr<int> p : count(j) = 0;
  return p; // expected-warning {{cannot prove return value bounds imply declared return bounds for 'f8'}} \
            // expected-note {{(expanded) inferred return value bounds are 'bounds(p, p + j)'}}
}

_Array_ptr<const char> f9(_Array_ptr<const char> p : bounds(p, (p + i) + 1), int i) : bounds(p, (p + i) + 1) { // expected-note {{(expanded) declared return bounds are 'bounds(p, (p + i) + 1)'}}
  _Array_ptr<const char> q : bounds(p, p + (i - 1)) = 0;
  return q; // expected-warning {{cannot prove return value bounds imply declared return bounds for 'f9'}} \
            // expected-note {{(expanded) inferred return value bounds are 'bounds(p, p + (i - 1))'}}
}

//
// Test bounds errors
//

_Array_ptr<long> f10(_Array_ptr<long> p : count(1)) : bounds(_Return_value, _Return_value + 2) { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + 2)'}}
  return p; // expected-error {{return value bounds do not imply declared return bounds for 'f10'}} \
            // expected-note {{declared return bounds are wider than the return value bounds}} \
            // expected-note {{declared return upper bound is above return value upper bound}} \
            // expected-note {{(expanded) inferred return value bounds are 'bounds(p, p + 1)'}}
}

_Array_ptr<int> f11(_Array_ptr<int> p : count(2)) : bounds(p - 1, p + 2) { // expected-note {{(expanded) declared return bounds are 'bounds(p - 1, p + 2)'}}
  return p; // expected-error {{return value bounds do not imply declared return bounds for 'f11'}} \
            // expected-note {{declared return bounds are wider than the return value bounds}} \
            // expected-note {{declared return lower bound is below return value lower bound}} \
            // expected-note {{(expanded) inferred return value bounds are 'bounds(p, p + 2)'}}
}

_Nt_array_ptr<int> f12(_Nt_array_ptr<int> p, // expected-note 2 {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + 3)'}}
                       _Nt_array_ptr<int> q : bounds(q + 1, q + 2),
                       int test) : count(3) {
  if (test)
    return p; // expected-error {{return value bounds do not imply declared return bounds for 'f12'}} \
              // expected-note {{source bounds are an empty range}} \
              // expected-note {{declared return upper bound is above return value upper bound}} \
              // expected-note {{(expanded) inferred return value bounds are 'bounds(p, p + 0)'}}
  else
    return q; // expected-error {{return value bounds do not imply declared return bounds for 'f12'}} \
              // expected-note {{declared return bounds are wider than the return value bounds}} \
              // expected-note {{declared return lower bound is below return value lower bound}} \
              // expected-note {{declared return upper bound is above return value upper bound}} \
              // expected-note {{(expanded) inferred return value bounds are 'bounds(q + 1, q + 2)'}}
}

//
// Test free variable bounds errors
//

_Array_ptr<char> f13(_Array_ptr<char> p : count(i), int i) : count(2) { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + 2)'}}
  return p; // expected-error {{it is not possible to prove that return value bounds imply declared return bounds for 'f13'}} \
            // expected-note {{the inferred upper bounds use the variable 'i' and there is no relational information involving 'i' and any of the expressions used by the declared upper bounds}} \
            // expected-note {{(expanded) inferred return value bounds are 'bounds(p, p + i)'}}
}

_Array_ptr<int> f14(_Array_ptr<int> p : count(3), // expected-note {{(expanded) declared return bounds are 'bounds(q, q + 3)'}}
                    _Array_ptr<int> q : bounds(p, p + 3)) : bounds(q, q + 3) {
  return q; // expected-error {{it is not possible to prove that return value bounds imply declared return bounds for 'f14'}} \
            // expected-note {{the declared bounds use the variable 'q' and there is no relational information involving 'q' and any of the expressions used by the inferred bounds}} \
            // expected-note {{the inferred bounds use the variable 'p' and there is no relational information involving 'p' and any of the expressions used by the declared bounds}} \
            // expected-note {{(expanded) inferred return value bounds are 'bounds(p, p + 3)'}}
}

_Nt_array_ptr<char> f15(int i) : count(i);

_Nt_array_ptr<char> f15(int i) : count(i) { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + i)'}}
  return "abc"; // expected-error {{it is not possible to prove that return value bounds imply declared return bounds for 'f15'}} \
                // expected-note {{the declared upper bounds use the variable 'i' and there is no relational information involving 'i' and any of the expressions used by the inferred upper bounds}} \
                // expected-note {{(expanded) inferred return value bounds are 'bounds(value of "abc", value of "abc" + 3)'}}
}
