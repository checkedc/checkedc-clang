// Tests for Non-Modifying Expressions with Checked C Extension
// This makes sure we raise errors when a programmer puts a modifying expression within a
// _Dynamic_check invocation.
//
// RUN: %clang_cc1 -fcheckedc-extension -verify %s

int f0(void);

struct S1 {
  int m1;
};

union U1 {
  int m1;
};

// Expressions explicitly banned by spec within Non-Modifying Expressions
void f1(int i) {
  // Conventional Assignment is a modifying expression
  _Dynamic_check(i = 1);       // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(1 + (i = 1)); // expected-error {{assignment expression not allowed in dynamic check expression}}

  // Compound Assignment is a modifying expression
  _Dynamic_check(i += 1);  // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(i -= 1);  // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(i *= 1);  // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(i /= 1);  // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(i %= 1);  // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(i <<= 1); // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(i >>= 1); // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(i &= 1);  // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(i ^= 1);  // expected-error {{assignment expression not allowed in dynamic check expression}}
  _Dynamic_check(i |= 1);  // expected-error {{assignment expression not allowed in dynamic check expression}}

  // Increments are modifying expressions
  _Dynamic_check(i++); // expected-error {{increment expression not allowed in dynamic check expression}}
  _Dynamic_check((i++, i)); // expected-error {{increment expression not allowed in dynamic check expression}}
  _Dynamic_check(++i); // expected-error {{increment expression not allowed in dynamic check expression}}
  _Dynamic_check(1 + 1 - 1 + ++i); // expected-error {{increment expression not allowed in dynamic check expression}}

  // Decrements are modifying expressions
  _Dynamic_check(i--); // expected-error {{decrement expression not allowed in dynamic check expression}}
  _Dynamic_check(--i); // expected-error {{decrement expression not allowed in dynamic check expression}}

  // Calls are modifying expressions
  _Dynamic_check(f0());     // expected-error {{call expression not allowed in dynamic check expression}}
  _Dynamic_check(1 + f0()); // expected-error {{call expression not allowed in dynamic check expression}}

  volatile int j;
  _Dynamic_check(j);     // expected-error {{volatile expression not allowed in dynamic check expression}}
  _Dynamic_check(i + j); // expected-error {{volatile expression not allowed in dynamic check expression}}
}

// Expressions explicitly allowed by spec within Non-Modifying Expressions
void f2(int i) {
  int j;

  // Local variables and Parameter Variables
  _Dynamic_check(i + j);

  // Constants
  _Dynamic_check(3);

  // Cast Expressions
  _Dynamic_check((int)'A');

  // Address-of Expressions
  _Dynamic_check(&i != &j);

  // Unary Plus/Minus Expressions
  _Dynamic_check(+i);
  _Dynamic_check(-i);

  // One's Complement Expressions
  _Dynamic_check(~i);

  // Logical Negation Expressions
  _Dynamic_check(!i);

  // Sizeof Expressions
  _Dynamic_check(sizeof(i) == sizeof(int));

  // Multiplicative Expressions
  _Dynamic_check(i * 1 / 1 % 1);

  // Additive Expressions
  _Dynamic_check(i + 1 - 1);

  // Shift Expressions
  _Dynamic_check(i << 1 >> 1);

  // Relational and Equality Expressions
  _Dynamic_check(i > j);
  _Dynamic_check(i < j);
  _Dynamic_check(i <= j);
  _Dynamic_check(i >= j);
  _Dynamic_check(i == j);

  // Bitwise Expressions
  _Dynamic_check(i & j);
  _Dynamic_check(i ^ j);
  _Dynamic_check(i | j);

  // Logical Expressions
  _Dynamic_check(i && j);
  _Dynamic_check(i || j);

  // Conditional Expressions
  _Dynamic_check(i ? j : 0);

  // Member references
  union U1 u1;
  struct S1 s1;
  _Dynamic_check(u1.m1);
  _Dynamic_check(s1.m1);

  // Indirect member references
  struct S1 *ps1;
  _Dynamic_check(ps1->m1);

  // Pointer Dereferences
  int *k;
  _Dynamic_check(*k);
}