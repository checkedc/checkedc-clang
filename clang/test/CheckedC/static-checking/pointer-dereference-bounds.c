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
  // The representative expression for the AbstractSet containing p[0], *p,
  // *(p + 0), and p[2 - 2] is *p.
  // The representative expression for the AbstractSet containing *(p + 2 + 3)
  // and 5[p] is 5[p].

  p[0] = *p + 1; // expected-error {{inferred bounds for '*p' are unknown after assignment}} \
                 // expected-note {{(expanded) declared bounds are 'bounds(*p, *p + 0)'}} \
                 // expected-note {{lost the value of the expression 'p[0]' which is used in the (expanded) inferred bounds 'bounds(*p, *p + 0)' of '*p'}}

  *(p + 0) = p[2 - 2] + 1; // expected-error {{inferred bounds for '*p' are unknown after assignment}} \
                           // expected-note {{(expanded) declared bounds are 'bounds(*p, *p + 0)'}} \
                           // expected-note {{lost the value of the expression '*(p + 0)' which is used in the (expanded) inferred bounds 'bounds(p[2 - 2], p[2 - 2] + 0)' of '*p'}}

  *(p + 2 + 3) = 5[p] - 2; // expected-error {{inferred bounds for '5[p]' are unknown after assignment}} \
                           // expected-note {{(expanded) declared bounds are 'bounds(5[p], 5[p] + 0)'}} \
                           // expected-note {{lost the value of the expression '*(p + 2 + 3)' which is used in the (expanded) inferred bounds 'bounds(5[p], 5[p] + 0)' of '5[p]'}}
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

struct S1 {
  _Array_ptr<int> f : bounds(arr[*len], arr[len[1]] + *(len + 2)); // expected-note 14 {{(expanded) declared bounds are 'bounds(s->arr[*s->len], s->arr[s->len[1]] + *(s->len + 2))'}}
  _Array_ptr<int> g : bounds(len, len + 0); // expected-note {{(expanded) declared bounds are 'bounds(s->len, s->len + 0)'}}
  _Array_ptr<int> *arr;
  _Array_ptr<int> len : count(10);
};

void f6(_Ptr<struct S1> s) {
  s->arr = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
              // expected-note {{lost the value of the expression 's->arr' which is used in the (expanded) inferred bounds 'bounds(s->arr[*s->len], s->arr[s->len[1]] + *(s->len + 2))' of 's->f'}}

  s->len = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
              // expected-note {{lost the value of the expression 's->len' which is used in the (expanded) inferred bounds 'bounds(s->arr[*s->len], s->arr[s->len[1]] + *(s->len + 2))' of 's->f'}} \
              // expected-error {{inferred bounds for 's->g' are unknown after assignment}} \
              // expected-note {{lost the value of the expression 's->len' which is used in the (expanded) inferred bounds 'bounds(s->len, s->len + 0)' of 's->g'}}
  
  // No members of s have bounds that depend on *s->arr.
  *s->arr = 0;

  // The bounds of s->f depend on the following dereferences/array subscripts:
  // 1. *len
  // 2. len[1]
  // 3. *(len + 2)
  // 4. arr[*len]
  // 5. arr[len[1]]

  // 1. Assigning to various forms of *len
  *s->len = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
               // expected-note {{lost the value of the expression '*s->len'}}

  *(s->len + 0) = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                     // expected-note {{lost the value of the expression '*(s->len + 0)'}}

  s->len[2 - 2] = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                     // expected-note {{lost the value of the expression 's->len[2 - 2]'}}

  // 2. Assigning to various forms of len[1]
  s->len[1] = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                 // expected-note {{lost the value of the expression 's->len[1]'}}

  *(s->len + 4 - 3) = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                         // expected-note {{lost the value of the expression '*(s->len + 4 - 3)'}}

  // 3. Assigning to various forms of *(len + 2)
  *(s->len + 2) = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                     // expected-note {{lost the value of the expression '*(s->len + 2)'}}

  2[s->len + 0] = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                     // expected-note {{lost the value of the expression '2[s->len + 0]'}}
  
  // 4. Assigning to various forms of arr[*len]
  s->arr[*s->len] = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                       // expected-note {{lost the value of the expression 's->arr[*s->len]'}}

  s->arr[*(s->len + 0)] = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                             // expected-note {{lost the value of the expression 's->arr[*(s->len + 0)]'}}

  *(s->arr + *s->len) = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                           // expected-note {{lost the value of the expression '*(s->arr + *s->len)'}}

  // 5. Assigning to various forms of arr[len[1]]
  s->arr[s->len[1]] = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                         // expected-note {{lost the value of the expression 's->arr[s->len[1]]'}}

  *(s->arr + s->len[1 + 0] + 0) = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
                                     // expected-note {{lost the value of the expression '*(s->arr + s->len[1 + 0] + 0)'}}
}

//
// Test pointer dereferences with bounds-safe interface types.
//

void f7(_Array_ptr<char *> p : count(10) itype(_Array_ptr<_Nt_array_ptr<char>>),
        _Nt_array_ptr<char> buf : bounds(unknown)) _Unchecked {
  // In an unchecked scope, *p has type char * and has target bounds of bounds(unknown).
  *p = buf;

  _Checked {
    // In a checked scope, *p has type _Nt_array_ptr<char> and has target bounds
    // of bounds(*p, *p + 0).
    *p = buf; // expected-error {{inferred bounds for '*p' are unknown after assignment}} \
              // expected-note {{(expanded) declared bounds are 'bounds(*p, *p + 0)'}} \
              // expected-note {{assigned expression 'buf' with unknown bounds to '*p'}}
  }
}
