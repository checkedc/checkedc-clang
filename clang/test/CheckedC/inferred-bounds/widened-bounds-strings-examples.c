// Tests for bounds widening for K&R string related functions.
//
// RUN: %clang_cc1 -fdump-widened-bounds -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s | FileCheck %s

// Return length of p (adapted from p. 39, K&R 2nd Edition).
// p implicitly has count(0).
int my_strlen(_Nt_array_ptr<char> p) {
  int i = 0;
  // Create a temporary whose count of elements
  // can change.
  _Nt_array_ptr<char> s : count(i) = p;
  // s[i] implies that the count can increase
  // by 1.
  while (s[i])
    ++i; // expected-error {{inferred bounds for 's' are unknown after increment}}
  return i;

// CHECK: In function: my_strlen
// CHECK:  [B3]
// CHECK:    1: ++i
// CHECK: upper_bound(s) = 1
}

// Delete all c from p (adapted from p. 47, K&R 2nd Edition)
// p implicltly has count(0).
void squeeze(_Nt_array_ptr<char> p, char c) {
  int i = 0, j = 0;
  // Create a temporary whose count of elements can
  // change.
  _Nt_array_ptr<char> s : count(i) = p;
  for ( ; s[i]; i++) { // expected-error {{inferred bounds for 's' are unknown after increment}} \
                       // expected-error {{inferred bounds for 'tmp' are unknown after increment}}
    // We will widen the bounds of s so that we
    // can assign to s[j] when j == i.
    _Nt_array_ptr<char> tmp : count(i + 1) = s;
    if (tmp[i] != c)
      tmp[j++] = tmp[i];
  }
  // if i==j, this writes a 0 at the upper bound.  Writing a 0 at the upper bound
  // is allowed for pointers to null-terminated arrays.  It is not allowed for
  // regular arrays.
  s[j] = 0;

// CHECK: In function: squeeze
// CHECK:  [B4]
// CHECK:    1: _Nt_array_ptr<char> tmp : count(i + 1) = s;
// CHECK:    2: tmp[i] != c
// CHECK: upper_bound(s) = 1

// CHECK:  [B3]
// CHECK:    1: tmp[j++] = tmp[i]
// CHECK: upper_bound(s) = 1

// CHECK:  [B2]
// CHECK:    1: i++
// CHECK: upper_bound(s) = 1
}

// Reverse a string in place (p. 62, K&R 2nd Edition).
// p implicitly has count(0).
void reverse(_Nt_array_ptr<char> p) {
  int len = 0;
  // Calculate the length of the string.
  _Nt_array_ptr<char> s : count(len) = p;
  for (; s[len]; len++); // expected-error {{inferred bounds for 's' are unknown after increment}}

  // Now that we know the length, use s just like we would use an array_ptr.
  for (int i = 0, j = len - 1; i < j; i++, j--) {
    int c = s[i];
    s[i] = s[j];
    s[j] = c;
  }

// CHECK: In function: reverse
// CHECK:  [B5]
// CHECK:    1: len++
// CHECK: upper_bound(s) = 1
}

// Return < 0 if s < t, 0 if s == t, > 0 if s > t.
// Adapted from p.106, K&R 2nd Edition.
// s and t implicitly have count(0).
int my_strcmp(_Nt_array_ptr<char> s, _Nt_array_ptr<char> t) {
  // Reading *s and *t is allowed for count(0)
  for (; *s == *t; s++, t++) // Incrementing s, t allowed because *s, *t != `\0`
    if (*s)
      return 0;
  return *s - *t;

// CHECK: In function: my_strcmp
// CHECK:  [B3]
// CHECK:    1: return 0;
// CHECK: upper_bound(s) = 1
}
