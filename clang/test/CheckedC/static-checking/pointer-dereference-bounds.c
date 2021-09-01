// Tests for checking:
// 1. Inferred bounds of pointer dereferences and array subscript expressions.
// 2. Bounds that use the value of a pointer dereference or array subscript.
//
// Because the static checker is mostly unimplemented, we only issue warnings
// when bounds declarations cannot be provided to hold.
//
// RUN: %clang_cc1 -fcheckedc-extension -Wcheck-bounds-decls -verify %s

//
// Test checking the bounds of pointer dereferences and array subscripts
// of type _Nt_array_ptr<T>.
//

extern _Nt_array_ptr<char> g1(_Nt_array_ptr<char> p);
extern _Nt_array_ptr<int> g2(_Nt_array_ptr<int> p);

void f1(_Array_ptr<_Nt_array_ptr<char>> ptr_to_buf : count(10),
        _Nt_array_ptr<char> buf : bounds(unknown)) {
  *ptr_to_buf = "abc";
  ptr_to_buf[0] = "xyz";

  *(ptr_to_buf + 5) = g1(*ptr_to_buf);
  5[ptr_to_buf] = g1(ptr_to_buf[1]);
  ptr_to_buf[7 - 2] = g1(*(ptr_to_buf + 2));

  // The representative expression for all these lvalues is *ptr_to_buf.
  *ptr_to_buf = buf; // expected-error {{inferred bounds for '*ptr_to_buf' are unknown after assignment}} \
                     // expected-note {{(expanded) declared bounds are 'bounds(*ptr_to_buf, *ptr_to_buf + 0)'}} \
                     // expected-note {{assigned expression 'buf' with unknown bounds to '*ptr_to_buf'}}
  0[ptr_to_buf] = buf; // expected-error {{inferred bounds for '*ptr_to_buf' are unknown after assignment}} \
                       // expected-note {{(expanded) declared bounds are 'bounds(*ptr_to_buf, *ptr_to_buf + 0)'}} \
                       // expected-note {{assigned expression 'buf' with unknown bounds to '*ptr_to_buf'}}
  *(ptr_to_buf + 2 - 1 - 1) = buf; // expected-error {{inferred bounds for '*ptr_to_buf' are unknown after assignment}} \
                                   // expected-note {{(expanded) declared bounds are 'bounds(*ptr_to_buf, *ptr_to_buf + 0)'}} \
                                   // expected-note {{assigned expression 'buf' with unknown bounds to '*ptr_to_buf'}}

  // The representative expression for all these lvalues is ptr_to_buf[4],
  // so the target bounds for each lvalue are created using ptr_to_buf[4].
  ptr_to_buf[4]++; // expected-warning {{cannot prove declared bounds for 'ptr_to_buf[4]' are valid after increment}} \
                   // expected-note {{(expanded) declared bounds are 'bounds(ptr_to_buf[4], ptr_to_buf[4] + 0)'}} \
                   // expected-note {{(expanded) inferred bounds are 'bounds(ptr_to_buf[4] - 1, ptr_to_buf[4] - 1 + 0)'}}
  ptr_to_buf[2 * 2] = ptr_to_buf[2 * 2] + 1; // expected-warning {{cannot prove declared bounds for 'ptr_to_buf[4]' are valid after assignment}} \
                       // expected-note {{(expanded) declared bounds are 'bounds(ptr_to_buf[4], ptr_to_buf[4] + 0)'}} \
                       // expected-note {{(expanded) inferred bounds are 'bounds(ptr_to_buf[2 * 2] - 1, ptr_to_buf[2 * 2] - 1 + 0)'}}
  *(1 + 3 + ptr_to_buf) += 1; // expected-warning {{cannot prove declared bounds for 'ptr_to_buf[4]' are valid after assignment}} \
                              // expected-note {{(expanded) declared bounds are 'bounds(ptr_to_buf[4], ptr_to_buf[4] + 0)'}} \
                              // expected-note {{(expanded) inferred bounds are 'bounds(*(1 + 3 + ptr_to_buf) - 1, *(1 + 3 + ptr_to_buf) - 1 + 0)'}}
}

// This test function demonstrates the fact that invertibility does not
// use semantic expression comparison, so expressions that might be expected
// to have an inverse actually have no inverse in the current implementation.
// TODO: investigate using semantic expression comparison in invertibility.
void f2(_Array_ptr<_Nt_array_ptr<char>> p : count(10)) {
  p[0] = *p + 1; // expected-error {{inferred bounds for 'p[0]' are unknown after assignment}} \
                 // expected-note {{(expanded) declared bounds are 'bounds(p[0], p[0] + 0)'}} \
                 // expected-note {{lost the value of the expression 'p[0]' which is used in the (expanded) inferred bounds 'bounds(*p, *p + 0)' of 'p[0]'}}

  *(p + 0) = p[2 - 2] + 1; // expected-error {{inferred bounds for 'p[0]' are unknown after assignment}} \
                           // expected-note {{(expanded) declared bounds are 'bounds(p[0], p[0] + 0)'}} \
                           // expected-note {{lost the value of the expression '*(p + 0)' which is used in the (expanded) inferred bounds 'bounds(p[2 - 2], p[2 - 2] + 0)' of 'p[0]'}}

  *(p + 2 + 3) = 5[p] - 2; // expected-error {{inferred bounds for '*(p + 2 + 3)' are unknown after assignment}} \
                           // expected-note {{(expanded) declared bounds are 'bounds(*(p + 2 + 3), *(p + 2 + 3) + 0)'}} \
                           // expected-note {{lost the value of the expression '*(p + 2 + 3)' which is used in the (expanded) inferred bounds 'bounds(5[p], 5[p] + 0)' of '*(p + 2 + 3)'}}
}

void f3(_Array_ptr<_Nt_array_ptr<int> *> p : itype(_Array_ptr<_Array_ptr<_Nt_array_ptr<int>>>) count(10),
        _Nt_array_ptr<int> val,
        _Nt_array_ptr<int> unknown : bounds(unknown),
        _Array_ptr<_Nt_array_ptr<int>> unknown_arr : bounds(unknown)) {
  // *p is an _Array_ptr so its target bounds are bounds(unknown).
  *p = unknown_arr;

  // **p is an _Nt_array_ptr so its target bounds are bounds(**p, **p + 0).
  // The RHS bounds are bounds(val, val + 0).
  **p = val;

  p[0][1] = unknown; // expected-error {{inferred bounds for 'p[0][1]' are unknown after assignment}} \
                     // expected-note {{(expanded) declared bounds are 'bounds(p[0][1], p[0][1] + 0)'}} \
                     // expected-note {{assigned expression 'unknown' with unknown bounds to 'p[0][1]'}}

  *(*(p + 2) + 3) = g2(*(*(p + 2) + 3));
}

//
// Test checking the bounds of pointer dereferences and array subscripts
// of type _Ptr<T>.
//

struct S {
  int len;
  _Ptr<struct S> next;
};

void f4(_Array_ptr<_Ptr<struct S>> s : count(10)) {
  *s = (*s)->next; // expected-error {{inferred bounds for '*s' are unknown after assignment}} \
                   // expected-note {{(expanded) declared bounds are 'bounds((_Array_ptr<struct S>)*s, (_Array_ptr<struct S>)*s + 1)'}} \
                   // expected-note {{lost the value of the expression '*s' which is used in the (expanded) inferred bounds 'bounds((_Array_ptr<struct S>)(*s)->next, (_Array_ptr<struct S>)(*s)->next + 1)' of '*s'}}

  // If we use a temporary variable to store (*s)->next, then the inferred
  // bounds of the RHS of the assignment to *s do not use the value of *s.
  _Ptr<struct S> temp = (*s)->next;
  *s = temp;
}

//
// Test modifying pointer dereferences and array subscripts that are used
// in the declared bounds of other lvalue expressions (variables, member
// expressions, etc).
//

void f5(_Array_ptr<int> p : count(*ptr_to_len), // expected-note 2 {{(expanded) declared bounds are 'bounds(p, p + *ptr_to_len)'}}
        _Array_ptr<unsigned int> ptr_to_len : count(10)) {
  *ptr_to_len = 0; // expected-error {{inferred bounds for 'p' are unknown after assignment}} \
                   // expected-note {{lost the value of the expression '*ptr_to_len' which is used in the (expanded) inferred bounds 'bounds(p, p + *ptr_to_len)' of 'p'}}

  ptr_to_len[0]++; // expected-warning {{cannot prove declared bounds for 'p' are valid after increment}} \
                   // expected-note {{(expanded) inferred bounds are 'bounds(p, p + ptr_to_len[0] - 1U)'}}
}
