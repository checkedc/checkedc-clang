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

//
// Test bounds-safe interfaces
//

int *f16(int *p) : byte_count(4) _Unchecked { // expected-note {{(expanded) declared return bounds are 'bounds((_Array_ptr<char>)_Return_value, (_Array_ptr<char>)_Return_value + 4)'}}
  return p; // expected-error {{return value has unknown bounds, bounds expected because the function 'f16' has bounds}}
}

int *f17(int *p : bounds(unknown)) : count(5) _Checked { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + 5)'}}
  return p; // expected-error {{return value has unknown bounds, bounds expected because the function 'f17' has bounds}}
}

int *f19(int *p : count(6)) : count(6) _Unchecked { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + 6)'}}
  return p + 1; // expected-warning {{cannot prove return value bounds imply declared return bounds for 'f19'}} \
                // expected-note {{(expanded) inferred return value bounds are 'bounds(p, p + 6)'}}
}

int *f20(int *p : count(7)) : bounds(p, p + 7) _Checked {
  return p + 1;
}

int *f21(_Array_ptr<int> p : count(8)) : count(8) _Unchecked {
  return p;
}

//
// Test bounds casts
//

_Array_ptr<char> f22(_Array_ptr<char> p : count(i), int i) : count(3) {
  return _Dynamic_bounds_cast<_Array_ptr<char>>(p, count(3));
}

_Array_ptr<int> f23(_Array_ptr<int> p : count(4), int i) : count(i) {
  return _Assume_bounds_cast<_Array_ptr<int>>(p, count(i));
}

_Array_ptr<int> f24(_Array_ptr<int> p : count(i), int i) : count(2) {
  return _Dynamic_bounds_cast<_Array_ptr<int>>(p + 1, count(2));
}

//
// Test function calls
//

extern _Array_ptr<int> g1(void) : count(2);
extern int *g2(int size) : count(size);
extern _Nt_array_ptr<char> g3(_Nt_array_ptr<char> p) : bounds(p, p);

_Array_ptr<int> f25(void) : count(2) {
  return g1();
}

_Array_ptr<int> f26(int len) : count(len + 1) {
  return g2(len + 1);
}

_Nt_array_ptr<char> f27(_Nt_array_ptr<char> p) : bounds(p, p) {
  return g3(p);
}
