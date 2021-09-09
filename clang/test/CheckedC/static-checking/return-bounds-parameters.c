// Tests for checking that parameter expressions used in declared return
// bounds are unmodified in checked scopes (or if the function's return
// type is a checked pointer).
//
// RUN: %clang_cc1 -fcheckedc-extension -Wcheck-bounds-decls -verify %s

//
// Test variable parameters used in return bounds.
//

_Array_ptr<int> f1(_Array_ptr<int> p : count(1), unsigned int i) : bounds(p, p + i) { // expected-note 2 {{(expanded) declared return bounds are 'bounds(p, p + i)'}}
  p = 0; // expected-error {{modified expression 'p' used in the declared return bounds for 'f1'}}
  i++; // expected-error {{modified expression 'i' used in the declared return bounds for 'f1'}}
  return 0;
}

_Nt_array_ptr<char> f2(unsigned int len) : count(len) { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + len)'}}
  len = len * 2; // expected-error {{modified expression 'len' used in the declared return bounds for 'f2'}}
  return 0;
}

_Array_ptr<int> f3(unsigned int a, unsigned int b) : count(a + b) { // expected-note 2 {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + a + b)'}}
  a++, b--; // expected-error {{modified expression 'a' used in the declared return bounds for 'f3'}} \
            // expected-error {{modified expression 'b' used in the declared return bounds for 'f3'}}
  return 0;
}

//
// Test variable parameters, pointer dereferences, and array subscripts
// used in return bounds.
//

_Array_ptr<int> f4(_Array_ptr<int> arr : count(1), unsigned int idx) : byte_count(arr[idx]) { // expected-note 5 {{(expanded) declared return bounds are 'bounds((_Array_ptr<char>)_Return_value, (_Array_ptr<char>)_Return_value + arr[idx])'}}
  arr = 0; // expected-error {{modified expression 'arr' used in the declared return bounds for 'f4'}}
  idx--; // expected-error {{modified expression 'idx' used in the declared return bounds for 'f4'}}

  arr[idx] = 1; // expected-error {{modified expression 'arr[idx]' used in the declared return bounds for 'f4'}}
  *(arr + idx) = 2; // expected-error {{'*(arr + idx)' used in the declared return bounds for 'f4'}}
  *(idx + arr + 1 - 1) = 3; // expected-error {{'*(idx + arr + 1 - 1)' used in the declared return bounds for 'f4'}}
  return 0;
}

_Array_ptr<char> f5(_Ptr<int> num) : count(*num + 1) { // expected-note 2 {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + *num + 1)'}}
  num = 0; // expected-error {{modified expression 'num' used in the declared return bounds for 'f5'}}

  *num = 1; // expected-error {{modified expression '*num' used in the declared return bounds for 'f5'}}
  return 0;
}

//
// Test member expressions used in return bounds.
//

struct S1 {
  _Array_ptr<int> f : count(2);
  int len;
};

_Array_ptr<char> f6(struct S1 *s) : count(s->len) { // expected-note {{(expanded) declared return bounds are 'bounds(_Return_value, _Return_value + s->len)'}}
  s->len *= 2; // expected-error {{modified expression 's->len' used in the declared return bounds for 'f6'}}
  return 0;
}

_Array_ptr<int> f7(struct S1 s) : bounds(s.f, s.f + s.len) { // expected-note 2 {{(expanded) declared return bounds are 'bounds(s.f, s.f + s.len)'}}
  s.f = 0; // expected-error {{modified expression 's.f' used in the declared return bounds for 'f7'}}
  s.len++; // expected-error {{modified expression 's.len' used in the declared return bounds for 'f7'}}
  return 0;
}

//
// Test funtions with bounds-safe interfaces in checked scopes
//

struct S2 {
  int *f : count(2);
  int len;
};

int *f8(struct S2 s, int i) : bounds(s.f, s.f + i) _Checked { // expected-note 2 {{(expanded) declared return bounds are 'bounds(s.f, s.f + i)'}}
  s.f = 0; // expected-error {{modified expression 's.f' used in the declared return bounds for 'f8'}}
  i += 1; // expected-error {{modified expression 'i' used in the declared return bounds for 'f8'}}
  return 0;
}

char *f9(_Array_ptr<char> p) : bounds(p, p + 1) _Checked { // expected-note {{(expanded) declared return bounds are 'bounds(p, p + 1)'}}
  p++; // expected-error {{modified expression 'p' used in the declared return bounds for 'f9'}}
  return 0;
}

//
// Test functions with bounds-safe interfaces in unchecked scopes
//

int *f10(_Array_ptr<int> p, _Array_ptr<int> q, unsigned int i) : bounds(p, p + i) _Unchecked {
  p = q;
  i = 0;
  return 0;
}

char *f11(struct S2 *s) : count(s->len + 1) _Unchecked {
  s->len += 2;
  return 0;
}

